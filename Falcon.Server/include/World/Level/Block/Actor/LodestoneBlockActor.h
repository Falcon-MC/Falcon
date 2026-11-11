#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>

class LodestoneBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Lodestone";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    // the PositionTrackingDB record of this lodestone, 0 until a compass links to it
    int32_t mTrackingId = 0;
};
