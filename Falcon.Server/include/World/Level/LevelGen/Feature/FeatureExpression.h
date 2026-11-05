#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// the Molang subset the overworld feature rules and scatter features write their scatter with: numbers, the
// variables and queries below, math.ceil and math.max, arithmetic, comparisons, && and || and the ternary;
// evaluated in floats like the game
class FeatureExpression {
public:
    class Context {
    public:
        virtual ~Context() = default;

        // variable.originx/y/z, variable.worldx/y/z, variable.is_legacy
        virtual float variable(const std::string &name) const = 0;

        // query.heightmap, query.above_top_solid and query.noise, each with two arguments
        virtual float query(const std::string &name, float first, float second) const = 0;
    };

    enum class Operation : uint8_t {
        Constant,
        Variable,
        Query,
        Ceil,
        Max,
        Negate,
        Not,
        Add,
        Subtract,
        Multiply,
        Divide,
        Less,
        LessOrEqual,
        Greater,
        GreaterOrEqual,
        Equal,
        NotEqual,
        And,
        Or,
        Ternary
    };

    struct Node {
        Operation mOperation = Operation::Constant;
        float mValue = 0.0f;
        std::string mName;
        int32_t mOperands[3] = {-1, -1, -1};
    };

    // a shared expression for the text, or nullptr when it uses anything outside the subset
    static std::shared_ptr<const FeatureExpression> parse(const std::string &text);

    float evaluate(const Context &context) const;

private:
    std::vector<Node> mNodes;
    int32_t mRoot = -1;

    float _evaluate(int32_t index, const Context &context) const;

    friend class FeatureExpressionParser;
};
