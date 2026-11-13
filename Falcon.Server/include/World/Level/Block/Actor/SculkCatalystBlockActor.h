#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Sculk/SculkSpreader.h"

class SculkCatalystBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "SculkCatalyst";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    SculkSpreader &getSpreader() {
        return mSpreader;
    }

private:
    SculkSpreader mSpreader;
};
