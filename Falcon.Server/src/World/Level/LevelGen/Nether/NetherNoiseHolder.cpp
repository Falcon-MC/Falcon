#include "World/Level/LevelGen/Nether/NetherNoiseHolder.h"

#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <random>
#include <vector>

namespace {
    NormalNoisePtr makeIdentical(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes) {
        const IRandomPtr copy = random.identical();
        return std::make_shared<NormalNoise>(*copy, firstOctave, amplitudes);
    }
}

NetherTerrainNoises::NetherTerrainNoises(uint32_t seed) {
    // one Mersenne twister seeded with the low half of the level seed feeds the stacks in this order
    std::mt19937 random(seed);
    mMinLimitNoise.reset(new TheEndOctaveNoise(random, 16));
    mMaxLimitNoise.reset(new TheEndOctaveNoise(random, 16));
    mMainNoise.reset(new TheEndOctaveNoise(random, 8));

    // this stack and the next two are only drawn: nothing reads them, but they move the twister
    TheEndOctaveNoise(random, 4);

    for (int32_t i = 0; i < 4; i++)
        mDepthNoise.emplace_back(random);

    TheEndOctaveNoise(random, 10);
    TheEndOctaveNoise(random, 16);

    for (int32_t i = 0; i < 3; i++)
        mAdjustmentNoise.emplace_back(random);
}

NetherDeltaNoises::NetherDeltaNoises(IRandom &random) {
    mSurfaceNoise = makeIdentical(random, -6, {1.0f, 1.0f, 1.0f});
    mSurfaceSecondaryNoise = makeIdentical(random, -6, {1.0f, 0.0f, 1.0f, 1.0f});
}

namespace {
    const NetherNoiseHolder *&sharedInstance() {
        static const NetherNoiseHolder *instance = nullptr;
        return instance;
    }
}

void NetherNoiseHolder::setInstance(const NetherNoiseHolder *instance) {
    sharedInstance() = instance;
}

const NetherNoiseHolder *NetherNoiseHolder::get() {
    return sharedInstance();
}

NetherNoiseHolder::NetherNoiseHolder(int64_t seed) {
    mRandomSource = std::make_shared<XoroshiroRandom>(seed);
    mTerrainNoises = std::make_unique<NetherTerrainNoises>((uint32_t) seed);
    mDeltaNoises = std::make_unique<NetherDeltaNoises>(*mRandomSource);
}
