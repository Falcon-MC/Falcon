#pragma once

#include "Block/Blocks/LiquidBlock.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <string>

class WaterBlock : public LiquidBlock {
public:
    explicit WaterBlock(const BlockState &state) : LiquidBlock(state) {}
    explicit WaterBlock(const LiquidBlock &block) : LiquidBlock(block) {}
    explicit WaterBlock(const Block &block) : LiquidBlock(block) {}

    static bool matches(const std::string &identifier);

    static BlockState source();

    static bool isSource(const BlockState &state);

    int getTickRate() const override { return 5; }
    int getFlowDecayPerBlock() const override { return 1; }
    int getMinAdjacentSourcesToFormSource() const override { return 2; }
    const char *getBucketFillSound() const override { return LevelSoundEvent::BUCKET_FILL_WATER; }
    const char *getBucketEmptySound() const override { return LevelSoundEvent::BUCKET_EMPTY_WATER; }
};
