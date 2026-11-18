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

    // flowing_*: turns still on its queued tick
    static bool isDynamic(const std::string &identifier);

    bool suppressesFreezing() const override {
        return true;
    }

    bool hasRandomTick() const override {
        return true;
    }

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    // ten tries for a flammable block in the 3x3x3 cube, fire on its first air neighbour
    static void spreadFire(LiquidPhysicsSystem &ticks, const Vector3i &position);

    // ultrawarm dimensions run lava three times faster
    int32_t getTickDelay(DimensionType dimension) const override;

    const char *getBucketFillSound() const override {
        return LevelSoundEvent::BUCKET_FILL_LAVA;
    }
    const char *getBucketEmptySound() const override {
        return LevelSoundEvent::BUCKET_EMPTY_LAVA;
    }
};
