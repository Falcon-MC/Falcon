#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class AbstractSlimeActor : public HostileActor {
public:
    static constexpr int LARGE_SIZE = 4;

    static constexpr int MEDIUM_SIZE = 2;

    static constexpr int SMALL_SIZE = 1;

    static constexpr float SIZE_SCALE = 0.52f;

    using HostileActor::HostileActor;

    bool preventsSleep() const override {
        return false;
    }

    int getSizeVariant() const { return mSizeVariant; }

    void setSizeVariant(int variant);

    ActorSize getSize() const override;

    float getDefaultMaxHealth() const override;

    int getExperienceDrop() const override { return mSizeVariant; }

    virtual float getContactDamage() const = 0;

    virtual float getHopPower() const {
        return BASE_HOP_POWER;
    }

    void setHopDirection(float yaw, bool aggressive) {
        mHopYaw = yaw;
        mAggressive = aggressive;
    }

    float getHopYaw() const {
        return mHopYaw;
    }

    void setHopSpeed(float speed) {
        mHopSpeed = speed;
    }

    void hop();

    static constexpr float BASE_HOP_POWER = 0.42f;

    void finalizeSpawn() override;

    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    void tick(ServerNetworkHandler &owner) override;

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

private:
    void _split(ServerNetworkHandler &owner, Level &level);

    void _attackTouchingPlayers(ServerNetworkHandler &owner);

    void _tickHop(ServerNetworkHandler &owner);

    int32_t _nextJumpDelay() const;

    int mSizeVariant = LARGE_SIZE;
    int32_t mAttackCooldown = 0;
    float mHopYaw = 0.0f;
    float mHopSpeed = 0.0f;
    bool mAggressive = false;
    int32_t mJumpDelay = 0;
};
