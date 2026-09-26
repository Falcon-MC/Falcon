#include "Actor/Definition/Molang.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerActor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <vector>

namespace {
    const char *const QUERY_PREFIX = "query.";
    const char *const QUERY_SHORT_PREFIX = "q.";

    std::mt19937 &molangRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    std::string lower(std::string value) {
        for (char &c: value)
            c = (char) std::tolower((unsigned char) c);
        return value;
    }

    bool equals(const MolangValue &left, const MolangValue &right) {
        if (left.mIsString || right.mIsString)
            return left.mIsString == right.mIsString && left.mString == right.mString;
        return left.mNumber == right.mNumber;
    }

    class Parser {
    public:
        Parser(const std::string &source, const ServerActor &actor) : mSource(source), mActor(actor) {
        }

        MolangValue parse() {
            return _ternary();
        }

    private:
        void _skipWhitespace() {
            while (mPosition < mSource.size() && std::isspace((unsigned char) mSource[mPosition]))
                mPosition++;
        }

        bool _accept(const char *token) {
            _skipWhitespace();
            const size_t length = std::char_traits<char>::length(token);
            if (mSource.compare(mPosition, length, token) != 0)
                return false;

            mPosition += length;
            return true;
        }

        bool _peek(char token) {
            _skipWhitespace();
            return mPosition < mSource.size() && mSource[mPosition] == token;
        }

        MolangValue _ternary() {
            MolangValue condition = _or();
            if (!_accept("?"))
                return condition;

            MolangValue whenTrue = _ternary();
            _accept(":");
            MolangValue whenFalse = _ternary();
            return condition.isTrue() ? whenTrue : whenFalse;
        }

        MolangValue _or() {
            MolangValue left = _and();
            while (_accept("||")) {
                const MolangValue right = _and();
                left = MolangValue::ofNumber(left.isTrue() || right.isTrue() ? 1.0 : 0.0);
            }
            return left;
        }

        MolangValue _and() {
            MolangValue left = _equality();
            while (_accept("&&")) {
                const MolangValue right = _equality();
                left = MolangValue::ofNumber(left.isTrue() && right.isTrue() ? 1.0 : 0.0);
            }
            return left;
        }

        MolangValue _equality() {
            MolangValue left = _comparison();
            while (true) {
                if (_accept("==")) {
                    left = MolangValue::ofNumber(equals(left, _comparison()) ? 1.0 : 0.0);
                } else if (_accept("!=")) {
                    left = MolangValue::ofNumber(equals(left, _comparison()) ? 0.0 : 1.0);
                } else {
                    return left;
                }
            }
        }

        MolangValue _comparison() {
            MolangValue left = _additive();
            while (true) {
                if (_accept("<=")) {
                    left = MolangValue::ofNumber(left.mNumber <= _additive().mNumber ? 1.0 : 0.0);
                } else if (_accept(">=")) {
                    left = MolangValue::ofNumber(left.mNumber >= _additive().mNumber ? 1.0 : 0.0);
                } else if (_accept("<")) {
                    left = MolangValue::ofNumber(left.mNumber < _additive().mNumber ? 1.0 : 0.0);
                } else if (_accept(">")) {
                    left = MolangValue::ofNumber(left.mNumber > _additive().mNumber ? 1.0 : 0.0);
                } else {
                    return left;
                }
            }
        }

        MolangValue _additive() {
            MolangValue left = _multiplicative();
            while (true) {
                if (_accept("+")) {
                    left = MolangValue::ofNumber(left.mNumber + _multiplicative().mNumber);
                } else if (_accept("-")) {
                    left = MolangValue::ofNumber(left.mNumber - _multiplicative().mNumber);
                } else {
                    return left;
                }
            }
        }

        MolangValue _multiplicative() {
            MolangValue left = _unary();
            while (true) {
                if (_accept("*")) {
                    left = MolangValue::ofNumber(left.mNumber * _unary().mNumber);
                } else if (_accept("/")) {
                    const double divisor = _unary().mNumber;
                    left = MolangValue::ofNumber(divisor == 0.0 ? 0.0 : left.mNumber / divisor);
                } else {
                    return left;
                }
            }
        }

        MolangValue _unary() {
            if (_accept("!"))
                return MolangValue::ofNumber(_unary().isTrue() ? 0.0 : 1.0);
            if (_accept("-"))
                return MolangValue::ofNumber(-_unary().mNumber);
            return _primary();
        }

        MolangValue _primary() {
            _skipWhitespace();
            if (mPosition >= mSource.size())
                return MolangValue::ofNumber(0.0);

            if (_accept("(")) {
                MolangValue value = _ternary();
                _accept(")");
                return value;
            }

            const char current = mSource[mPosition];
            if (current == '\'')
                return _string();
            if (std::isdigit((unsigned char) current) || current == '.')
                return _number();
            if (std::isalpha((unsigned char) current) || current == '_')
                return _identifier();

            mPosition++;
            return MolangValue::ofNumber(0.0);
        }

        MolangValue _string() {
            const size_t start = ++mPosition;
            while (mPosition < mSource.size() && mSource[mPosition] != '\'')
                mPosition++;

            std::string value = mSource.substr(start, mPosition - start);
            if (mPosition < mSource.size())
                mPosition++;
            return MolangValue::ofString(std::move(value));
        }

        MolangValue _number() {
            const size_t start = mPosition;
            while (mPosition < mSource.size()
                   && (std::isdigit((unsigned char) mSource[mPosition]) || mSource[mPosition] == '.'))
                mPosition++;

            const double value = std::strtod(mSource.substr(start, mPosition - start).c_str(), nullptr);
            if (mPosition < mSource.size() && (mSource[mPosition] == 'f' || mSource[mPosition] == 'F'))
                mPosition++;
            return MolangValue::ofNumber(value);
        }

        MolangValue _identifier() {
            const size_t start = mPosition;
            while (mPosition < mSource.size()
                   && (std::isalnum((unsigned char) mSource[mPosition]) || mSource[mPosition] == '_'
                       || mSource[mPosition] == '.'))
                mPosition++;

            const std::string name = lower(mSource.substr(start, mPosition - start));
            if (name == "true")
                return MolangValue::ofNumber(1.0);
            if (name == "false")
                return MolangValue::ofNumber(0.0);

            std::vector<MolangValue> arguments;
            if (_accept("(")) {
                if (!_accept(")")) {
                    do {
                        arguments.push_back(_ternary());
                    } while (_accept(","));
                    _accept(")");
                }
            }

            return _call(name, arguments);
        }

        static double _argument(const std::vector<MolangValue> &arguments, size_t index) {
            return index < arguments.size() ? arguments[index].mNumber : 0.0;
        }

        MolangValue _call(const std::string &name, const std::vector<MolangValue> &arguments) const {
            std::string query;
            if (name.rfind(QUERY_PREFIX, 0) == 0)
                query = name.substr(std::char_traits<char>::length(QUERY_PREFIX));
            else if (name.rfind(QUERY_SHORT_PREFIX, 0) == 0)
                query = name.substr(std::char_traits<char>::length(QUERY_SHORT_PREFIX));

            if (!query.empty())
                return _query(query, arguments);

            if (name == "math.clamp")
                return MolangValue::ofNumber(std::min(std::max(_argument(arguments, 0), _argument(arguments, 1)),
                                                      _argument(arguments, 2)));
            if (name == "math.min")
                return MolangValue::ofNumber(std::min(_argument(arguments, 0), _argument(arguments, 1)));
            if (name == "math.max")
                return MolangValue::ofNumber(std::max(_argument(arguments, 0), _argument(arguments, 1)));
            if (name == "math.abs")
                return MolangValue::ofNumber(std::fabs(_argument(arguments, 0)));
            if (name == "math.floor")
                return MolangValue::ofNumber(std::floor(_argument(arguments, 0)));
            if (name == "math.ceil")
                return MolangValue::ofNumber(std::ceil(_argument(arguments, 0)));
            if (name == "math.round")
                return MolangValue::ofNumber(std::round(_argument(arguments, 0)));
            if (name == "math.trunc")
                return MolangValue::ofNumber(std::trunc(_argument(arguments, 0)));
            if (name == "math.mod") {
                const double divisor = _argument(arguments, 1);
                return MolangValue::ofNumber(divisor == 0.0 ? 0.0 : std::fmod(_argument(arguments, 0), divisor));
            }
            if (name == "math.random") {
                const double low = _argument(arguments, 0);
                const double high = _argument(arguments, 1);
                return MolangValue::ofNumber(high <= low ? low
                                                         : std::uniform_real_distribution<double>(low, high)(
                                                                 molangRandom()));
            }
            if (name == "math.random_integer") {
                const int64_t low = (int64_t) std::round(_argument(arguments, 0));
                const int64_t high = (int64_t) std::round(_argument(arguments, 1));
                return MolangValue::ofNumber(high <= low ? (double) low
                                                         : (double) std::uniform_int_distribution<int64_t>(low, high)(
                                                                 molangRandom()));
            }

            return MolangValue::ofNumber(0.0);
        }

        MolangValue _query(const std::string &query, const std::vector<MolangValue> &arguments) const {
            const std::string argument = arguments.empty() ? std::string() : arguments.front().mString;

            if (query == "property")
                return _property(argument);

            if (query == "has_property")
                return MolangValue::ofNumber(mActor.findPropertyDescription(argument) != nullptr ? 1.0 : 0.0);

            if (query == "had_component_group") {
                const MobActor *mob = dynamic_cast<const MobActor *>(&mActor);
                return MolangValue::ofNumber(mob != nullptr && mob->hasComponentGroup(argument) ? 1.0 : 0.0);
            }

            return MolangValue::ofNumber(0.0);
        }

        MolangValue _property(const std::string &name) const {
            const ActorPropertyDescription *descriptor = mActor.findPropertyDescription(name);
            if (descriptor == nullptr)
                return MolangValue::ofNumber(0.0);

            if (descriptor->mType == ActorPropertyDescription::Type::Float)
                return MolangValue::ofNumber(mActor.getFloatProperty(name, descriptor->mDefaultFloat));

            const int32_t value = mActor.getIntProperty(name, descriptor->mDefaultInt);
            if (descriptor->mType != ActorPropertyDescription::Type::Enum)
                return MolangValue::ofNumber((double) value);

            if (value >= 0 && value < (int32_t) descriptor->mEnumValues.size())
                return MolangValue::ofString(descriptor->mEnumValues[(size_t) value]);
            return MolangValue::ofString(std::string());
        }

        const std::string &mSource;
        const ServerActor &mActor;
        size_t mPosition = 0;
    };
}

MolangValue MolangValue::ofNumber(double number) {
    MolangValue value;
    value.mNumber = number;
    return value;
}

MolangValue MolangValue::ofString(std::string string) {
    MolangValue value;
    value.mString = std::move(string);
    value.mIsString = true;
    return value;
}

bool MolangValue::isTrue() const {
    return mIsString ? !mString.empty() : mNumber != 0.0;
}

MolangValue Molang::evaluate(const std::string &expression, const ServerActor &actor) {
    Parser parser(expression, actor);
    return parser.parse();
}
