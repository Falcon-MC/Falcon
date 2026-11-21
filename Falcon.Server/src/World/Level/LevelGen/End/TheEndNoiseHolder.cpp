#include "World/Level/LevelGen/End/TheEndNoiseHolder.h"


#include <random>

TheEndTerrainNoises::TheEndTerrainNoises(int64_t seed) {
    // the game seeds its Mersenne twister with the low half of the level seed and draws the stacks in this order
    std::mt19937 random((uint32_t) seed);
    mRoughnessNoiseOctaves.reset(new TheEndOctaveNoise(random, 16));
    mRoughnessNoiseOctaves2.reset(new TheEndOctaveNoise(random, 16));
    mDetailNoiseOctaves.reset(new TheEndOctaveNoise(random, 8));
    mIslandNoise.reset(new TheEndSimplexNoise(random));
}

TheEndNoiseHolder::TheEndNoiseHolder(int64_t seed) {
    mTerrainNoises.reset(new TheEndTerrainNoises(seed));
}
