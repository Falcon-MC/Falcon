#pragma once

#include "Actor/ServerActor.h"

class EndCrystalActor final : public ServerActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:ender_crystal";

    static constexpr double EXPLOSION_SIZE = 6.0;

    EndCrystalActor(uint64_t runtimeId, const std::string &identifier);

    ActorSize getSize() const override { return ActorSize{0.98f, 0.98f}; }

    void tick(ServerNetworkHandler &owner) override;

    bool isExpired() const override { return mExpired; }

    bool onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) override;

private:
    bool mExpired = false;
};
