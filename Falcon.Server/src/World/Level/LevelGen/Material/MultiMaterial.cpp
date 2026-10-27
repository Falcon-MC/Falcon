#include "World/Level/LevelGen/Material/MultiMaterial.h"

#include <utility>

MultiMaterial::MultiMaterial(std::vector<MaterialRule> rules) : mRules(std::move(rules)) {
}

const BlockState *MultiMaterial::calculate(FunctionContext &context) const {
    for (const MaterialRule &rule: mRules) {
        if (!rule)
            continue;

        const BlockState *state = rule(context);
        if (state != nullptr)
            return state;
    }

    return nullptr;
}

const std::vector<MaterialRule> &MultiMaterial::rules() const {
    return mRules;
}

MaterialRule MultiMaterial::toRule() const {
    MultiMaterial copy = *this;
    return [copy](FunctionContext &context) -> const BlockState * {
        return copy.calculate(context);
    };
}
