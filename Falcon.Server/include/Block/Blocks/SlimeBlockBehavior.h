#pragma once

#include "Block/Components/BlockBehavior.h"

class SlimeBlockBehavior final : public BlockBehavior {
public:
    float getFrictionFactor() const override { return 0.8f; }

    float getLandingBounce() const override { return 1.0f; }

    bool slowsWalking() const override { return true; }

    bool onEntityLand(Actor &actor, float downwardVelocity) const override;

    std::optional<float> getFallDamage(const Actor &actor, float vanillaFallDamage) const override;
};
