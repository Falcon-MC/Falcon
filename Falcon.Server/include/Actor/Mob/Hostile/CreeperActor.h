#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

#include <cstdint>

class CreeperActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:creeper";
    static constexpr int32_t MAX_SWELL = 30;

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.8f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    void tick(ServerNetworkHandler &owner) override;

    bool isExpired() const override {
        return mExploded || MobActor::isExpired();
    }

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;

    void onStruckByLightning(ServerNetworkHandler &owner) override;

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    bool isPowered() const {
        return mPowered;
    }

    bool isIgnited() const {
        return mIgnited;
    }

    int32_t getSwell() const {
        return mSwell;
    }

    void setSwellDirection(int32_t direction) {
        mSwellDirection = direction;
    }

private:
    void _syncFuse(ServerNetworkHandler &owner);

    void _explode(ServerNetworkHandler &owner);

    void _dropChargedHead(ServerNetworkHandler &owner, const std::vector<int64_t> &candidates);

    int32_t mSwell = 0;
    int32_t mSwellDirection = -1;
    bool mIgnited = false;
    bool mPowered = false;
    bool mExploded = false;
    bool mFuseVisible = false;
};
