#include "World/Level/LevelGen/End/TheEndNoiseHolder.h"

#include "World/Level/LevelGen/Random/SimpleRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

TheEndTerrainNoises::TheEndTerrainNoises(int64_t seed)
        : mRoughnessNoiseOctaves(), mRoughnessNoiseOctaves2(), mDetailNoiseOctaves(), mIslandNoise() {
    XoroshiroRandom roughnessRandom(seed);
    mRoughnessNoiseOctaves.reset(new TheEndOctaveNoise(roughnessRandom, 16));

    XoroshiroRandom roughnessRandom2(seed);
    mRoughnessNoiseOctaves2.reset(new TheEndOctaveNoise(roughnessRandom2, 16));

    XoroshiroRandom detailRandom(seed);
    mDetailNoiseOctaves.reset(new TheEndOctaveNoise(detailRandom, 8));

    XoroshiroRandom islandRandom(seed);
    mIslandNoise.reset(new TheEndSimplexNoise(islandRandom));
}

TheEndNoiseHolder::TheEndNoiseHolder(int64_t seed)
        : mTerrainNoises(), mDecorationIslandNoise() {
    mTerrainNoises.reset(new TheEndTerrainNoises(seed));

    SimpleRandom decorationRandom(seed);
    mDecorationIslandNoise.reset(new TheEndSimplexNoise(decorationRandom));
}
