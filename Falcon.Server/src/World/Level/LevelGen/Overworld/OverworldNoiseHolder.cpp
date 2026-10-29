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
    NormalNoisePtr makeForked(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes) {
        const IRandomPtr forked = random.fork();
        return std::make_shared<NormalNoise>(*forked, firstOctave, amplitudes);
    }

    NormalNoisePtr makeIdentical(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes) {
        const IRandomPtr copy = random.identical();
        return std::make_shared<NormalNoise>(*copy, firstOctave, amplitudes);
    }
}

OverworldBiomeNoises::OverworldBiomeNoises(IRandom &random) {
    mContinentalNoise = makeForked(random, -9, {1.0f, 1.0f, 2.0f, 2.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f});
    mTemperatureNoise = makeForked(random, -10, {1.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f});
    mHumidityNoise = makeForked(random, -8, {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    mErosionNoise = makeForked(random, -9, {1.0f, 1.0f, 0.0f, 1.0f, 1.0f});
    mWeirdnessNoise = makeForked(random, -7, {1.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f});
    mOffsetNoise = makeForked(random, -3, {1.0f, 1.0f, 1.0f, 0.0f});
    mJaggedNoise = makeForked(random, -16, {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});
}

OverworldTerrainNoises::OverworldTerrainNoises(IRandom &random, const OverworldBiomeNoises &biomeNoises) {
    mSurfaceNoise = makeIdentical(random, -6, {1.0f, 1.0f, 1.0f});
    mJagged = makeIdentical(random, -16, {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
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

    const IRandomPtr base3dRandom = random.identical();
    mBase3d = DensityBase3dNoise::overworld(*base3dRandom);

    mBarrierNoise = makeForked(random, -3, {1.0f, 1.0f, 1.0f});
    mFluidLevelFloodednessNoise = makeForked(random, -7, {1.0f, 1.0f, 0.0f, 1.0f});
    mFluidLevelSpreadNoise = makeForked(random, -5, {1.0f, 1.0f, 1.0f});
    mLavaNoise = makeForked(random, -1, {1.0f, 1.0f});
    mPillar = makeForked(random, -7, {1.0f, 1.0f});
    mPillarRareness = makeForked(random, -8, {1.0f});
    mPillarThickness = makeForked(random, -8, {1.0f});
    mSpaghetti2d = makeForked(random, -7, {1.0f});
    mSpaghetti2dElevation = makeForked(random, -8, {1.0f});
    mSpaghetti2dModulator = makeForked(random, -11, {1.0f});
    mSpaghetti2dThickness = makeForked(random, -11, {1.0f});
    mSpaghetti3dFirst = makeForked(random, -7, {1.0f});
    mSpaghetti3dSecond = makeForked(random, -7, {1.0f});
    mSpaghetti3dRarity = makeForked(random, -11, {1.0f});
    mSpaghetti3dThickness = makeForked(random, -8, {1.0f});
    mSpaghettiRoughness = makeForked(random, -5, {1.0f});
    mSpaghettiRoughnessModulator = makeForked(random, -8, {1.0f});
    mCaveEntrance = makeForked(random, -7, {0.4f, 0.5f, 1.0f});
    mCaveLayer = makeForked(random, -8, {1.0f});
    mCaveCheese = makeForked(random, -8, {0.5f, 1.0f, 2.0f, 1.0f, 2.0f, 1.0f, 0.0f, 2.0f, 0.0f});
    mNoodle = makeForked(random, -8, {1.0f});
    mNoodleThickness = makeForked(random, -8, {1.0f});
    mNoodleRidgeA = makeForked(random, -7, {1.0f});
    mNoodleRidgeB = makeForked(random, -7, {1.0f});
    mVeinToggleNoise = makeForked(random, -8, {1.0f});
    mVeinANoise = makeForked(random, -7, {1.0f});
    mVeinBNoise = makeForked(random, -7, {1.0f});
    mOreGapNoise = makeForked(random, -5, {1.0f});

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

OverworldNoiseHolder::OverworldNoiseHolder(int64_t seed) {
    mRandomSource = std::make_shared<XoroshiroRandom>(seed);
    mBiomeNoises = std::make_unique<OverworldBiomeNoises>(*mRandomSource);
    mTerrainNoises = std::make_unique<OverworldTerrainNoises>(*mRandomSource, *mBiomeNoises);
}
