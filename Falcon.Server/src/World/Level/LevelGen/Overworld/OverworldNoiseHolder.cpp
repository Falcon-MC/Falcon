#include "World/Level/LevelGen/Overworld/OverworldNoiseHolder.h"

#include "World/Level/LevelGen/Density/DensityBase3dNoise.h"
#include "World/Level/LevelGen/Density/DensityCommon.h"
#include "World/Level/LevelGen/Overworld/Density/DensityContinents.h"
#include "World/Level/LevelGen/Overworld/Density/DensityDepth.h"
#include "World/Level/LevelGen/Overworld/Density/DensityErosion.h"
#include "World/Level/LevelGen/Overworld/Density/DensityFactor.h"
#include "World/Level/LevelGen/Overworld/Density/DensityJaggedness.h"
#include "World/Level/LevelGen/Overworld/Density/DensityOffset.h"
#include "World/Level/LevelGen/Overworld/Density/DensityOreVeins.h"
#include "World/Level/LevelGen/Overworld/Density/DensityRidges.h"
#include "World/Level/LevelGen/Overworld/Density/DensityRidgesFolded.h"
#include "World/Level/LevelGen/Overworld/Density/DensitySlopedCheese.h"
#include "World/Level/LevelGen/Overworld/Density/OverworldCavesDensity.h"
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
    mJaggedNoise = NormalNoise::create(noiseFactory, "minecraft:jagged", -16, {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});
}

OverworldTerrainNoises::OverworldTerrainNoises(IRandom &random, const XoroshiroPositionalFactory &noiseFactory,
                                               const OverworldBiomeNoises &biomeNoises) {
    mSurfaceNoise = NormalNoise::create(noiseFactory, "minecraft:surface", -6, {1.0f, 1.0f, 1.0f});
    mJagged = NormalNoise::create(noiseFactory, "minecraft:jagged", -16, {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                          1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});

    const NormalNoisePtr shiftNoise = biomeNoises.getOffsetNoise();
    const NormalNoisePtr continentalness = biomeNoises.getContinentalNoise();
    const NormalNoisePtr erosionNoise = biomeNoises.getErosionNoise();
    const NormalNoisePtr ridgeNoise = biomeNoises.getWeirdnessNoise();
    const NormalNoisePtr jaggedNoise = biomeNoises.getJaggedNoise();

    mContinents = DensityContinents::overworldContinents(continentalness, shiftNoise);
    mErosion = DensityErosion::overworldErosion(erosionNoise, shiftNoise);
    mRidges = DensityRidges::overworldRidges(ridgeNoise, shiftNoise);
    mRidgesFolded = DensityRidgesFolded::overworldRidgesFolded(mRidges);

    mOffset = DensityOffset::overworldOffset(mContinents, mErosion, mRidgesFolded);
    mDepth = DensityDepth::overworldDepth(mOffset);
    mFactor = DensityFactor::overworldFactor(mContinents, mErosion, mRidges, mRidgesFolded);
    mJaggedness = DensityJaggedness::overworldJaggedness(mContinents, mErosion, mRidges, mRidgesFolded);

    mBase3d = DensityBase3dNoise::overworld(noiseFactory);

    mBarrierNoise = NormalNoise::create(noiseFactory, "minecraft:aquifer_barrier", -3, {1.0f});
    mFluidLevelFloodednessNoise = NormalNoise::create(noiseFactory, "minecraft:aquifer_fluid_level_floodedness", -7, {1.0f});
    mFluidLevelSpreadNoise = NormalNoise::create(noiseFactory, "minecraft:aquifer_fluid_level_spread", -5, {1.0f});
    mLavaNoise = NormalNoise::create(noiseFactory, "minecraft:aquifer_lava", -1, {1.0f});
    mPillar = NormalNoise::create(noiseFactory, "minecraft:pillar", -7, {1.0f, 1.0f});
    mPillarRareness = NormalNoise::create(noiseFactory, "minecraft:pillar_rareness", -8, {1.0f});
    mPillarThickness = NormalNoise::create(noiseFactory, "minecraft:pillar_thickness", -8, {1.0f});
    mSpaghetti2d = NormalNoise::create(noiseFactory, "minecraft:spaghetti_2d", -7, {1.0f});
    mSpaghetti2dElevation = NormalNoise::create(noiseFactory, "minecraft:spaghetti_2d_elevation", -8, {1.0f});
    mSpaghetti2dModulator = NormalNoise::create(noiseFactory, "minecraft:spaghetti_2d_modulator", -11, {1.0f});
    mSpaghetti2dThickness = NormalNoise::create(noiseFactory, "minecraft:spaghetti_2d_thickness", -11, {1.0f});
    mSpaghetti3dFirst = NormalNoise::create(noiseFactory, "minecraft:spaghetti_3d_1", -7, {1.0f});
    mSpaghetti3dSecond = NormalNoise::create(noiseFactory, "minecraft:spaghetti_3d_2", -7, {1.0f});
    mSpaghetti3dRarity = NormalNoise::create(noiseFactory, "minecraft:spaghetti_3d_rarity", -11, {1.0f});
    mSpaghetti3dThickness = NormalNoise::create(noiseFactory, "minecraft:spaghetti_3d_thickness", -8, {1.0f});
    mSpaghettiRoughness = NormalNoise::create(noiseFactory, "minecraft:spaghetti_roughness", -5, {1.0f});
    mSpaghettiRoughnessModulator = NormalNoise::create(noiseFactory, "minecraft:spaghetti_roughness_modulator", -8, {1.0f});
    mCaveEntrance = NormalNoise::create(noiseFactory, "minecraft:cave_entrance", -7, {0.4f, 0.5f, 1.0f});
    mCaveLayer = NormalNoise::create(noiseFactory, "minecraft:cave_layer", -8, {1.0f});
    mCaveCheese = NormalNoise::create(noiseFactory, "minecraft:cave_cheese", -8, {0.5f, 1.0f, 2.0f, 1.0f, 2.0f, 1.0f, 0.0f, 2.0f, 0.0f});
    mNoodle = NormalNoise::create(noiseFactory, "minecraft:noodle", -8, {1.0f});
    mNoodleThickness = NormalNoise::create(noiseFactory, "minecraft:noodle_thickness", -8, {1.0f});
    mNoodleRidgeA = NormalNoise::create(noiseFactory, "minecraft:noodle_ridge_a", -7, {1.0f});
    mNoodleRidgeB = NormalNoise::create(noiseFactory, "minecraft:noodle_ridge_b", -7, {1.0f});
    mVeinToggleNoise = NormalNoise::create(noiseFactory, "minecraft:ore_veininess", -8, {1.0f});
    mVeinANoise = NormalNoise::create(noiseFactory, "minecraft:ore_vein_a", -7, {1.0f});
    mVeinBNoise = NormalNoise::create(noiseFactory, "minecraft:ore_vein_b", -7, {1.0f});
    mOreGapNoise = NormalNoise::create(noiseFactory, "minecraft:ore_gap", -5, {1.0f});

    mSlopedCheese = DensitySlopedCheese::overworldSlopedCheese(mDepth, mJaggedness, mFactor, mBase3d, jaggedNoise);

    mDensityFunction = OverworldCavesDensity::finalDensity(
            mSlopedCheese,
            mSpaghettiRoughness,
            mSpaghettiRoughnessModulator,
            mSpaghetti2dThickness,
            mSpaghetti2dModulator,
            mSpaghetti2d,
            mSpaghetti2dElevation,
            mSpaghetti3dRarity,
            mSpaghetti3dThickness,
            mSpaghetti3dFirst,
            mSpaghetti3dSecond,
            mCaveEntrance,
            mCaveLayer,
            mCaveCheese,
            mPillar,
            mPillarRareness,
            mPillarThickness,
            mNoodle,
            mNoodleThickness,
            mNoodleRidgeA,
            mNoodleRidgeB
    );

    mPreliminarySurfaceDensity = OverworldCavesDensity::preliminarySurfaceLevel(mOffset, mFactor);
    mPreliminarySurfaceUpperBound = OverworldCavesDensity::preliminarySurfaceLevelUpperBound(mOffset, mFactor);

    mVeinToggle = DensityOreVeins::overworldVeinToggle(mVeinToggleNoise);
    mVeinRidged = DensityOreVeins::overworldVeinRidged(mVeinANoise, mVeinBNoise);
    mVeinGap = DensityOreVeins::overworldVeinGap(mOreGapNoise);

    mOreVeinifierSeed = random.nextLong();

    mWrappedDensity = DensityCommon::cacheAllInCell(mDensityFunction);
}

OverworldNoiseHolder::OverworldNoiseHolder(int64_t seed)
    : mRandomSource(std::make_shared<XoroshiroRandom>(seed)), mNoiseFactory(createNoiseFactory(seed)) {
    mBiomeNoises = std::make_unique<OverworldBiomeNoises>(mNoiseFactory);
    mTerrainNoises = std::make_unique<OverworldTerrainNoises>(*mRandomSource, mNoiseFactory, *mBiomeNoises);
}
