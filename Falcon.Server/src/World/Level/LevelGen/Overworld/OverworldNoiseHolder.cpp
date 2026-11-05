#include "World/Level/LevelGen/Overworld/OverworldNoiseHolder.h"

#include "World/Level/LevelGen/Overworld/Density/DensityContinents.h"
#include "World/Level/LevelGen/Overworld/Density/DensityErosion.h"
#include "World/Level/LevelGen/Overworld/Density/DensityRidges.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

namespace {
    XoroshiroPositionalFactory createNoiseFactory(int64_t seed) {
        XoroshiroRandom random(seed);
        return random.forkPositional();
    }
}

OverworldBiomeNoises::OverworldBiomeNoises(const XoroshiroPositionalFactory &noiseFactory) {
    mContinentalNoise = NormalNoise::create(noiseFactory, "minecraft:continentalness", -9, {1.0f, 1.0f, 2.0f, 2.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f});
    mTemperatureNoise = NormalNoise::create(noiseFactory, "minecraft:temperature", -10, {1.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f});
    mHumidityNoise = NormalNoise::create(noiseFactory, "minecraft:vegetation", -8, {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    mErosionNoise = NormalNoise::create(noiseFactory, "minecraft:erosion", -9, {1.0f, 1.0f, 0.0f, 1.0f, 1.0f});
    mWeirdnessNoise = NormalNoise::create(noiseFactory, "minecraft:ridge", -7, {1.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f});
    mOffsetNoise = NormalNoise::create(noiseFactory, "minecraft:offset", -3, {1.0f, 1.0f, 1.0f, 0.0f});
}

OverworldTerrainNoises::OverworldTerrainNoises(const OverworldBiomeNoises &biomeNoises) {
    const NormalNoisePtr shiftNoise = biomeNoises.getOffsetNoise();
    mContinents = DensityContinents::overworldContinents(biomeNoises.getContinentalNoise(), shiftNoise);
    mErosion = DensityErosion::overworldErosion(biomeNoises.getErosionNoise(), shiftNoise);
    mRidges = DensityRidges::overworldRidges(biomeNoises.getWeirdnessNoise(), shiftNoise);
}

OverworldNoiseHolder::OverworldNoiseHolder(int64_t seed)
    : mRandomSource(std::make_shared<XoroshiroRandom>(seed)), mNoiseFactory(createNoiseFactory(seed)) {
    mBiomeNoises = std::make_unique<OverworldBiomeNoises>(mNoiseFactory);
    mTerrainNoises = std::make_unique<OverworldTerrainNoises>(*mBiomeNoises);
}
