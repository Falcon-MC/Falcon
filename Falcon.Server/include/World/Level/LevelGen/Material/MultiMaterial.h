#pragma once

#include "World/Level/LevelGen/Material/MaterialRule.h"

#include <vector>

class MultiMaterial {
public:
    explicit MultiMaterial(std::vector<MaterialRule> rules);

    const BlockState *calculate(FunctionContext &context) const;

    const std::vector<MaterialRule> &rules() const;

    MaterialRule toRule() const;

private:
    std::vector<MaterialRule> mRules;
};
