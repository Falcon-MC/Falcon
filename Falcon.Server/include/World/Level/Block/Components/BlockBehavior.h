#pragma once

#include <optional>

class Actor;

class BlockBehavior {
public:
    virtual ~BlockBehavior() = default;

    virtual float getFrictionFactor() const { return 0.6f; }

    virtual float getLandingBounce() const { return 0.0f; }

    virtual bool slowsWalking() const { return false; }

    virtual bool slidesAlongSides() const { return false; }

    virtual bool onEntityLand(Actor &actor, float downwardVelocity) const;

    virtual std::optional<float> getFallDamage(const Actor &actor, float vanillaFallDamage) const;
};
