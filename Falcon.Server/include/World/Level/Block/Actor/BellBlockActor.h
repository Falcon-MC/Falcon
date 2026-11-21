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

    // BellBlockActor::ring: the swing starts over towards direction
    void ring(int32_t direction) {
        mTicks = 0;
        mRinging = true;
        mDirection = direction;
    }

private:
    int32_t mDirection = 0;
    bool mRinging = false;
    int32_t mTicks = 0;
};
