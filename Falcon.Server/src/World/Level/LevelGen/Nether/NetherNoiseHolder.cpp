#include "World/Level/LevelGen/Nether/NetherNoiseHolder.h"

#include "World/Level/LevelGen/Nether/Density/DensityNether.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <vector>

namespace {
    NormalNoisePtr makeIdentical(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes) {
        const IRandomPtr copy = random.identical();
        return std::make_shared<NormalNoise>(*copy, firstOctave, amplitudes);
    }
}

NetherTerrainNoises::NetherTerrainNoises(IRandom &random) {
    mSurfaceNoise = makeIdentical(random, -6, {1.0f, 1.0f, 1.0f});
    mPatchNoise = makeIdentical(random, -5, {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.013333333333333334f});
    mSoulsandNoise = makeIdentical(random, -8, {1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                0.013333333333333334f});
    mNetherStateNoise = makeIdentical(random, -4, {1.0f});
    mNetherwartNoise = makeIdentical(random, -3, {1.0f, 0.0f, 0.0f, 0.9f});

    const IRandomPtr base3dRandom = random.identical();
    mBase3dNoise = DensityNether::base3dNoise(*base3dRandom);
    mDensityFunction = DensityNether::finalDensity(mBase3dNoise);
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
    mTerrainNoises = std::make_unique<NetherTerrainNoises>(*mRandomSource);
    mDeltaNoises = std::make_unique<NetherDeltaNoises>(*mRandomSource);
}
