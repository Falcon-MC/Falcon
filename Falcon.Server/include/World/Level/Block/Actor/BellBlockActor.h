#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>

class BellBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Bell";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    // the swing counts two a tick while it rings, so a ring lasts 25 ticks
    static constexpr int32_t RING_TICKS = 50;
    // ticks before a ring sends the mobs around into hiding again
    static constexpr int32_t ALERT_COOLDOWN = 300;

    bool tick(ServerNetworkHandler &owner) override;

    // the swing starts over towards direction
    void ring(int32_t direction) {
        mTicks = 0;
        mRinging = true;
        mDirection = direction;
    }

    bool isRinging() const {
        return mRinging;
    }

    int32_t getDirection() const {
        return mDirection;
    }

    bool canAlert() const {
        return mAlertCooldown <= 0;
    }

    void startAlertCooldown() {
        mAlertCooldown = ALERT_COOLDOWN;
    }

    // whether the last redstone update found it powered; a ring wants the rise
    bool isPowered() const {
        return mPowered;
    }

    void setPowered(bool powered) {
        mPowered = powered;
    }

private:
    int32_t mDirection = 0;
    bool mRinging = false;
    int32_t mTicks = 0;
    int32_t mAlertCooldown = 0;
    bool mPowered = false;
};
