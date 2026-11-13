#pragma once

#include "World/Item/Item.h"

#include <string>

class ServerActor;

class ThrowableItem : public Item {
public:
    // None: the dispenser drops it; Shoot: launch event only; ShootWithClick: block.click first, as
    // the game does for data-driven projectiles
    enum class DispenseMode { None, Shoot, ShootWithClick };

    ThrowableItem(const Item &base, std::string entityIdentifier, float throwForce, int32_t cooldownTicks,
                  DispenseMode dispenseMode);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool dispense(const DispenseSource &source) const override;

protected:
    // the shot entity, null when the definition refused it
    ServerActor *_shootFrom(const DispenseSource &source) const;

    std::string mEntityIdentifier;
    float mThrowForce;
    int32_t mCooldownTicks;
    DispenseMode mDispenseMode;
};
