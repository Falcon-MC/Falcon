#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

#include <cmath>

class HayBlockBehavior final : public BlockBehavior {
public:
    // a hay bale keeps a fifth of the fall damage
    std::optional<float> getFallDamage(const Actor &actor, float vanillaFallDamage) const override {
        (void) actor;
        return std::ceil(vanillaFallDamage * 0.2f);
    }
};
