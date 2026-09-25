#pragma once

#include "Actor/Mob/MobActor.h"

#include <cstdint>
#include <string>

class PluginActor : public MobActor {
public:
    PluginActor(uint64_t runtimeId, const std::string &identifier);

    ActorCategory getCategory() const override;

    ActorSize getSize() const override;

    float getDefaultMaxHealth() const override;

    void tick(ServerNetworkHandler &owner) override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;
};
