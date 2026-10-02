#pragma once

#include "Block/BlockState.h"

#include <cstdint>

class BlockManager;
class IRandom;
class LevelChunk;
class SimplexNoise;

namespace IceCavesSupport {

    const SimplexNoise *gradientNoise();

    const BlockState *gradientState(float noise);

    const BlockState &packedIceState();

    const BlockState &iceCrystalState();

    const BlockState &snowLayerState();

    BlockState icicleState(int32_t index, int32_t lastIndex);

    bool isNaturalBaseBlock(const BlockState &state);

    bool isIceCaves(LevelChunk &chunk, int32_t x, int32_t y, int32_t z);

    int32_t triangle(IRandom &random, int32_t min, int32_t max);

    bool findFloor(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t range, int32_t &floorY);

    bool findCeiling(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t range, int32_t &ceilingY);

    void placeIcicle(BlockManager &manager, int32_t x, int32_t ceilingY, int32_t z, int32_t maxLength);

}
