#pragma once

#include <cstdint>
#include <random>

class BlockManager;
class LevelChunk;

// the features the nether generator places itself around the biome rules, from one random (sub_14CDAD2C0)
class NetherDecoration {
public:
    NetherDecoration(BlockManager &blocks, const LevelChunk &chunk, uint32_t levelSeed);

    void placeBeforeBiomes();

    void placeAfterBiomes();

private:
    int32_t _at(int32_t origin, int32_t bound);

    void _placeSpring(int32_t x, int32_t y, int32_t z, bool closed);

    void _placeFire(int32_t x, int32_t y, int32_t z);

    void _placeGlowstone(int32_t x, int32_t y, int32_t z);

    void _placeMushrooms(int32_t x, int32_t y, int32_t z, const class BlockState &mushroom);

    void _placeDebris(int32_t x, int32_t y, int32_t z, int32_t maxCount);

    BlockManager &mBlocks;
    std::mt19937 mRandom;
    uint32_t mDecorationSeed;
    int32_t mBaseX;
    int32_t mBaseZ;
};
