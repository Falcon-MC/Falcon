#pragma once

#include "World/Actor/Mob/Hostile/HostileActor.h"

#include <cstdint>

class EndermiteActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:endermite";
    static constexpr int32_t MAX_LIFETIME = 2400;

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.4f, 0.3f}; }

    float getDefaultMaxHealth() const override { return 8.0f; }

    int getExperienceDrop() const override { return 3; }

    void tick(ServerNetworkHandler &owner) override;

    // true once the endermite has lived out its lifetime and must despawn
    bool advanceLifetime();

    int32_t getLifetime() const {
        return mLifetime;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

private:
    int32_t mLifetime = 0;
};
