#include "World/Level/LevelGen/Feature/FeatureExpression.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

class FeatureExpressionParser {
public:
    FeatureExpressionParser(const std::string &text, FeatureExpression &expression)
            : mText(text), mExpression(expression) {}

    bool parse() {
        mExpression.mRoot = _ternary();
        _skipSpaces();
        return mValid && mPosition == mText.size();
    }

private:
    using Operation = FeatureExpression::Operation;

    const std::string &mText;
    FeatureExpression &mExpression;
    size_t mPosition = 0;
    bool mValid = true;

    int32_t _node(Operation operation, int32_t first = -1, int32_t second = -1, int32_t third = -1) {
        FeatureExpression::Node node;
        node.mOperation = operation;
        node.mOperands[0] = first;
        node.mOperands[1] = second;
        node.mOperands[2] = third;
        mExpression.mNodes.push_back(node);
        return (int32_t) mExpression.mNodes.size() - 1;
    }

    void _skipSpaces() {
        while (mPosition < mText.size() && std::isspace((unsigned char) mText[mPosition]))
            mPosition++;
    }

    bool _accept(const char *token) {
        _skipSpaces();
        const size_t length = std::char_traits<char>::length(token);
        if (mText.compare(mPosition, length, token) != 0)
            return false;

        mPosition += length;
        return true;
    }

    void _expect(const char *token) {
        if (!_accept(token))
            mValid = false;
    }

    int32_t _ternary() {
        const int32_t condition = _or();
        if (!_accept("?"))
            return condition;

        const int32_t whenTrue = _ternary();
        _expect(":");
        const int32_t whenFalse = _ternary();
        return _node(Operation::Ternary, condition, whenTrue, whenFalse);
    }

    int32_t _or() {
        int32_t left = _and();
        while (_accept("||"))
            left = _node(Operation::Or, left, _and());
        return left;
    }

    int32_t _and() {
        int32_t left = _comparison();
        while (_accept("&&"))
            left = _node(Operation::And, left, _comparison());
        return left;
    }

    int32_t _comparison() {
        const int32_t left = _sum();
        if (_accept("<="))
            return _node(Operation::LessOrEqual, left, _sum());
        if (_accept(">="))
            return _node(Operation::GreaterOrEqual, left, _sum());
        if (_accept("=="))
            return _node(Operation::Equal, left, _sum());
        if (_accept("!="))
            return _node(Operation::NotEqual, left, _sum());
        if (_accept("<"))
            return _node(Operation::Less, left, _sum());
        if (_accept(">"))
            return _node(Operation::Greater, left, _sum());
        return left;
    }

    int32_t _sum() {
        int32_t left = _product();
        while (mValid) {
            if (_accept("+"))
                left = _node(Operation::Add, left, _product());
            else if (_accept("-"))
                left = _node(Operation::Subtract, left, _product());
            else
                break;
        }
        return left;
    }

    int32_t _product() {
        int32_t left = _unary();
        while (mValid) {
            if (_accept("*"))
                left = _node(Operation::Multiply, left, _unary());
            else if (_accept("/"))
                left = _node(Operation::Divide, left, _unary());
            else
                break;
        }
        return left;
    }

    int32_t _unary() {
        if (_accept("-"))
            return _node(Operation::Negate, _unary());
        if (_accept("!"))
            return _node(Operation::Not, _unary());
        return _primary();
    }

    std::string _name() {
        _skipSpaces();
        const size_t start = mPosition;
        while (mPosition < mText.size()
               && (std::isalnum((unsigned char) mText[mPosition]) || mText[mPosition] == '_'
                   || mText[mPosition] == '.'))
            mPosition++;

        // Molang names ignore case: the packs write both math.random and Math.Random
        std::string name = mText.substr(start, mPosition - start);
        for (char &character: name)
            character = (char) std::tolower((unsigned char) character);
        return name;
    }

    // between two numbers the game folds the bounds when it compiles the call (sub_1416A29C0, opcodes 0x21 and
    // 0x22): rounds the draw differently from the call between two expressions
    int32_t _random(Operation operation, int32_t first, int32_t second) {
        const int32_t index = _node(operation, first, second);
        if (!mValid)
            return index;

        const FeatureExpression::Node &low = mExpression.mNodes[(size_t) first];
        const FeatureExpression::Node &high = mExpression.mNodes[(size_t) second];
        if (low.mOperation != Operation::Constant || high.mOperation != Operation::Constant)
            return index;

        FeatureExpression::Node &node = mExpression.mNodes[(size_t) index];
        node.mValue = std::fmin(high.mValue, low.mValue);
        node.mSecondValue = std::fmax(high.mValue, low.mValue);
        node.mFolded = true;
        return index;
    }

    // the tag is a single-quoted string
    int32_t _biomeTag() {
        _expect("'");
        const size_t end = mText.find('\'', mPosition);
        if (!mValid || end == std::string::npos) {
            mValid = false;
            return -1;
        }

        const int32_t index = _node(Operation::BiomeTag);
        mExpression.mNodes[(size_t) index].mName = mText.substr(mPosition, end - mPosition);
        mPosition = end + 1;
        _expect(")");
        return index;
    }

    int32_t _primary() {
        if (_accept("(")) {
            const int32_t inner = _ternary();
            _expect(")");
            return inner;
        }

        _skipSpaces();
        if (mPosition < mText.size() && (std::isdigit((unsigned char) mText[mPosition]) || mText[mPosition] == '.')) {
            char *end = nullptr;
            errno = 0;
            const float value = std::strtof(mText.c_str() + mPosition, &end);
            if (errno != 0 || end == mText.c_str() + mPosition) {
                mValid = false;
                return -1;
            }

            mPosition = (size_t) (end - mText.c_str());
            const int32_t index = _node(Operation::Constant);
            mExpression.mNodes[(size_t) index].mValue = value;
            return index;
        }

        const std::string name = _name();
        if (name.rfind("variable.", 0) == 0 || name.rfind("v.", 0) == 0) {
            const int32_t index = _node(Operation::Variable);
            mExpression.mNodes[(size_t) index].mName = name.substr(name.find('.') + 1);
            return index;
        }

        std::vector<int32_t> arguments;
        _expect("(");
        if (name == "query.has_biome_tag" || name == "q.has_biome_tag")
            return _biomeTag();
        if (!_accept(")")) {
            do
                arguments.push_back(_ternary());
            while (mValid && _accept(","));
            _expect(")");
        }

        if (name == "math.ceil" && arguments.size() == 1)
            return _node(Operation::Ceil, arguments[0]);
        if (name == "math.max" && arguments.size() == 2)
            return _node(Operation::Max, arguments[0], arguments[1]);
        if ((name == "math.random" || name == "math.random_integer") && arguments.size() == 2)
            return _random(name == "math.random" ? Operation::Random : Operation::RandomInteger, arguments[0],
                           arguments[1]);
        if ((name.rfind("query.", 0) == 0 || name.rfind("q.", 0) == 0) && arguments.size() == 2) {
            const int32_t index = _node(Operation::Query, arguments[0], arguments[1]);
            mExpression.mNodes[(size_t) index].mName = name.substr(name.find('.') + 1);
            return index;
        }

        mValid = false;
        return -1;
    }
};

std::shared_ptr<const FeatureExpression> FeatureExpression::parse(const std::string &text) {
    // the chunk workers ask for the same few texts over and over: they share the lock until a new text shows up
    static std::shared_mutex mutex;
    static std::unordered_map<std::string, std::shared_ptr<const FeatureExpression>> cache;

    {
        const std::shared_lock<std::shared_mutex> lock(mutex);
        const auto found = cache.find(text);
        if (found != cache.end())
            return found->second;
    }

    std::shared_ptr<FeatureExpression> expression = std::make_shared<FeatureExpression>();
    FeatureExpressionParser parser(text, *expression);
    std::shared_ptr<const FeatureExpression> result;
    if (!text.empty() && parser.parse())
        result = expression;

    const std::unique_lock<std::shared_mutex> lock(mutex);
    return cache.emplace(text, result).first->second;
}

float FeatureExpression::evaluate(const Context &context) const {
    return _evaluate(mRoot, context);
}

float FeatureExpression::_evaluate(int32_t index, const Context &context) const {
    const Node &node = mNodes[(size_t) index];
    const auto operand = [&](int32_t which) {
        return _evaluate(node.mOperands[which], context);
    };

    switch (node.mOperation) {
        case Operation::Constant:
            return node.mValue;
        case Operation::Variable:
            return context.variable(node.mName);
        case Operation::Query:
            return context.query(node.mName, operand(0), operand(1));
        case Operation::BiomeTag:
            return context.hasBiomeTag(node.mName) ? 1.0f : 0.0f;
        case Operation::Ceil:
            return std::ceil(operand(0));
        case Operation::Max:
            return std::fmax(operand(0), operand(1));
        case Operation::Random:
            return _random(node, context);
        case Operation::RandomInteger:
            return _randomInteger(node, context);
        case Operation::Negate:
            return -operand(0);
        case Operation::Not:
            return operand(0) == 0.0f ? 1.0f : 0.0f;
        case Operation::Add:
            return operand(0) + operand(1);
        case Operation::Subtract:
            return operand(0) - operand(1);
        case Operation::Multiply:
            return operand(0) * operand(1);
        case Operation::Divide:
            return operand(0) / operand(1);
        case Operation::Less:
            return operand(0) < operand(1) ? 1.0f : 0.0f;
        case Operation::LessOrEqual:
            return operand(0) <= operand(1) ? 1.0f : 0.0f;
        case Operation::Greater:
            return operand(0) > operand(1) ? 1.0f : 0.0f;
        case Operation::GreaterOrEqual:
            return operand(0) >= operand(1) ? 1.0f : 0.0f;
        case Operation::Equal:
            return operand(0) == operand(1) ? 1.0f : 0.0f;
        case Operation::NotEqual:
            return operand(0) != operand(1) ? 1.0f : 0.0f;
        case Operation::And:
            return operand(0) != 0.0f && operand(1) != 0.0f ? 1.0f : 0.0f;
        case Operation::Or:
            return operand(0) != 0.0f || operand(1) != 0.0f ? 1.0f : 0.0f;
        case Operation::Ternary:
            return operand(0) != 0.0f ? operand(1) : operand(2);
    }
    return 0.0f;
}

float FeatureExpression::_random(const Node &node, const Context &context) const {
    // sub_1416E2C00 between two numbers, sub_1416E2F30 between two expressions evaluated before the draw
    if (node.mFolded)
        return context.random() * (node.mSecondValue - node.mValue) + node.mValue;

    const float first = _evaluate(node.mOperands[0], context);
    const float second = _evaluate(node.mOperands[1], context);
    const float draw = context.random();
    return std::fmax(first, second) * draw + (1.0f - draw) * std::fmin(second, first);
}

float FeatureExpression::_randomInteger(const Node &node, const Context &context) const {
    // sub_1416E3270 and sub_1416E3580: the upper bound pushed just under the next integer so it can be floored to
    float low = node.mValue;
    float high = node.mSecondValue;
    if (!node.mFolded) {
        const float first = _evaluate(node.mOperands[0], context);
        const float second = _evaluate(node.mOperands[1], context);
        low = std::fmin(second, first);
        high = std::fmax(first, second);
    }

    const float above = (-1.1920929e-7f * high + 1.0f) + high;
    const float draw = context.random();
    const float value = std::floor(above * draw + (1.0f - draw) * low);
    return high < value ? high : std::fmax(value, low);
}
