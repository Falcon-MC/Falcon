#pragma once

#include "Block/Blocks/LiquidBlock.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <string>

class LavaBlock : public LiquidBlock {
public:
    explicit LavaBlock(const BlockState &state) : LiquidBlock(state) {}
    explicit LavaBlock(const LiquidBlock &block) : LiquidBlock(block) {}
    explicit LavaBlock(const Block &block) : LiquidBlock(block) {}

    static bool matches(const std::string &identifier);

    int getTickRate() const override { return 30; }
    int getFlowDecayPerBlock() const override { return 2; }
    int getMinAdjacentSourcesToFormSource() const override { return 0; }
    const char *getBucketFillSound() const override { return LevelSoundEvent::BUCKET_FILL_LAVA; }
    const char *getBucketEmptySound() const override { return LevelSoundEvent::BUCKET_EMPTY_LAVA; }
};
