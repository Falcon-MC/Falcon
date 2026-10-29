#pragma once

#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Material/MaterialRule.h"
#include "World/Level/LevelGen/Material/MultiMaterial.h"
#include "World/Level/LevelGen/Material/OreVeinifier.h"
#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <memory>

class OverworldBiomeNoises {
public:
    explicit OverworldBiomeNoises(IRandom &random);

    const NormalNoisePtr &getContinentalNoise() const { return mContinentalNoise; }

    const NormalNoisePtr &getTemperatureNoise() const { return mTemperatureNoise; }

    const NormalNoisePtr &getHumidityNoise() const { return mHumidityNoise; }

    const NormalNoisePtr &getErosionNoise() const { return mErosionNoise; }

    const NormalNoisePtr &getWeirdnessNoise() const { return mWeirdnessNoise; }

    const NormalNoisePtr &getOffsetNoise() const { return mOffsetNoise; }

    const NormalNoisePtr &getJaggedNoise() const { return mJaggedNoise; }

private:
    NormalNoisePtr mContinentalNoise;
    NormalNoisePtr mTemperatureNoise;
    NormalNoisePtr mHumidityNoise;
    NormalNoisePtr mErosionNoise;
    NormalNoisePtr mWeirdnessNoise;
    NormalNoisePtr mOffsetNoise;
    NormalNoisePtr mJaggedNoise;
};

class OverworldTerrainNoises {
public:
    OverworldTerrainNoises(IRandom &random, const OverworldBiomeNoises &biomeNoises);

    const DensityFunctionPtr &getContinents() const { return mContinents; }

    const DensityFunctionPtr &getErosion() const { return mErosion; }

    const DensityFunctionPtr &getRidges() const { return mRidges; }

    const DensityFunctionPtr &getRidgesFolded() const { return mRidgesFolded; }

    const DensityFunctionPtr &getOffset() const { return mOffset; }

    const DensityFunctionPtr &getDepth() const { return mDepth; }

    const DensityFunctionPtr &getFactor() const { return mFactor; }

    const DensityFunctionPtr &getJaggedness() const { return mJaggedness; }

    const DensityFunctionPtr &getDensityFunction() const { return mDensityFunction; }

    const DensityFunctionPtr &getPreliminarySurfaceDensity() const { return mPreliminarySurfaceDensity; }

    const DensityFunctionPtr &getPreliminarySurfaceUpperBound() const { return mPreliminarySurfaceUpperBound; }

    const DensityFunctionPtr &getWrappedDensity() const { return mWrappedDensity; }

    const NormalNoisePtr &getBarrierNoise() const { return mBarrierNoise; }

    const NormalNoisePtr &getFluidLevelFloodednessNoise() const { return mFluidLevelFloodednessNoise; }

    const NormalNoisePtr &getFluidLevelSpreadNoise() const { return mFluidLevelSpreadNoise; }

    const NormalNoisePtr &getLavaNoise() const { return mLavaNoise; }

    const DensityFunctionPtr &getVeinToggle() const { return mVeinToggle; }

    const DensityFunctionPtr &getVeinRidged() const { return mVeinRidged; }

    const DensityFunctionPtr &getVeinGap() const { return mVeinGap; }

    int64_t getOreVeinifierSeed() const { return mOreVeinifierSeed; }

private:
    NormalNoisePtr mSurfaceNoise;
    NormalNoisePtr mJagged;
    DensityFunctionPtr mContinents;
    DensityFunctionPtr mErosion;
    DensityFunctionPtr mRidges;
    DensityFunctionPtr mRidgesFolded;
    DensityFunctionPtr mOffset;
    DensityFunctionPtr mDepth;
    DensityFunctionPtr mFactor;
    DensityFunctionPtr mJaggedness;
    DensityFunctionPtr mBase3d;
    DensityFunctionPtr mSlopedCheese;
    DensityFunctionPtr mDensityFunction;
    DensityFunctionPtr mPreliminarySurfaceDensity;
    DensityFunctionPtr mPreliminarySurfaceUpperBound;
    DensityFunctionPtr mWrappedDensity;
    DensityFunctionPtr mVeinToggle;
    DensityFunctionPtr mVeinRidged;
    DensityFunctionPtr mVeinGap;

    NormalNoisePtr mBarrierNoise;
    NormalNoisePtr mFluidLevelFloodednessNoise;
    NormalNoisePtr mFluidLevelSpreadNoise;
    NormalNoisePtr mLavaNoise;
    NormalNoisePtr mPillar;
    NormalNoisePtr mPillarRareness;
    NormalNoisePtr mPillarThickness;
    NormalNoisePtr mSpaghetti2d;
    NormalNoisePtr mSpaghetti2dElevation;
    NormalNoisePtr mSpaghetti2dModulator;
    NormalNoisePtr mSpaghetti2dThickness;
    NormalNoisePtr mSpaghetti3dFirst;
    NormalNoisePtr mSpaghetti3dSecond;
    NormalNoisePtr mSpaghetti3dRarity;
    NormalNoisePtr mSpaghetti3dThickness;
    NormalNoisePtr mSpaghettiRoughness;
    NormalNoisePtr mSpaghettiRoughnessModulator;
    NormalNoisePtr mCaveEntrance;
    NormalNoisePtr mCaveLayer;
    NormalNoisePtr mCaveCheese;
    NormalNoisePtr mNoodle;
    NormalNoisePtr mNoodleThickness;
    NormalNoisePtr mNoodleRidgeA;
    NormalNoisePtr mNoodleRidgeB;
    NormalNoisePtr mVeinToggleNoise;
    NormalNoisePtr mVeinANoise;
    NormalNoisePtr mVeinBNoise;
    NormalNoisePtr mOreGapNoise;

    int64_t mOreVeinifierSeed = 0;
};

class OverworldNoiseHolder {
public:
    explicit OverworldNoiseHolder(int64_t seed);

    const OverworldBiomeNoises &getBiomeNoises() const { return *mBiomeNoises; }

    const OverworldTerrainNoises &getTerrainNoises() const { return *mTerrainNoises; }

    IRandom &getRandomSource() const { return *mRandomSource; }

private:
    IRandomPtr mRandomSource;
    std::unique_ptr<OverworldBiomeNoises> mBiomeNoises;
    std::unique_ptr<OverworldTerrainNoises> mTerrainNoises;
};
