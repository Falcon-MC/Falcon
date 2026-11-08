#pragma once

#include "World/Level/Block/LiquidBlock.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <string>

class LavaBlock : public LiquidBlock {
public:
    explicit LavaBlock(const BlockState &state) : LiquidBlock(state) {}
    explicit LavaBlock(const LiquidBlock &block) : LiquidBlock(block) {}
    explicit LavaBlock(const Block &block) : LiquidBlock(block) {}

    static bool matches(const std::string &identifier);

    // flowing_*: turns still on its queued tick (sub_149C43290)
    static bool isDynamic(const std::string &identifier);

    bool suppressesFreezing() const override {
        return true;
    }

    // ultrawarm dimensions run lava three times faster
    int32_t getTickDelay(DimensionType dimension) const override;

    const char *getBucketFillSound() const override { return LevelSoundEvent::BUCKET_FILL_LAVA; }
    const char *getBucketEmptySound() const override { return LevelSoundEvent::BUCKET_EMPTY_LAVA; }
};
