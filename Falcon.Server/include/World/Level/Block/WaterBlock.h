#pragma once

#include "World/Level/Block/LiquidBlock.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <string>

class WaterBlock : public LiquidBlock {
public:
    explicit WaterBlock(const BlockState &state) : LiquidBlock(state) {}
    explicit WaterBlock(const LiquidBlock &block) : LiquidBlock(block) {}
    explicit WaterBlock(const Block &block) : LiquidBlock(block) {}

    static bool matches(const std::string &identifier);

    // flowing_*: turns still on its queued tick (sub_149C43290)
    static bool isDynamic(const std::string &identifier);

    static BlockState source();

    static bool isSource(const BlockState &state);

    int32_t getTickDelay(DimensionType dimension) const override;

    const char *getBucketFillSound() const override { return LevelSoundEvent::BUCKET_FILL_WATER; }
    const char *getBucketEmptySound() const override { return LevelSoundEvent::BUCKET_EMPTY_WATER; }
};
