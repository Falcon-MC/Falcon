#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct BiomeClimateData {
    float mTemperature = 0.0f;
    float mDownfall = 0.0f;
    float mSnowAccumulationMin = 0.0f;
    float mSnowAccumulationMax = 0.0f;
};

struct BiomeCoordinateData {
    int32_t mMinValueType = 0;
    int16_t mMinValue = 0;
    int32_t mMaxValueType = 0;
    int16_t mMaxValue = 0;
    uint32_t mGridOffset = 0;
    uint32_t mGridStepSize = 0;
    int32_t mDistribution = 0;
};

struct BiomeScatterParamData {
    std::vector<BiomeCoordinateData> mCoordinates;
    int32_t mEvalOrder = 0;
    int32_t mChancePercentType = 0;
    int16_t mChancePercent = 0;
    int32_t mChanceNumerator = 0;
    int32_t mChanceDenominator = 0;
    int32_t mIterationsType = 0;
    int16_t mIterations = 0;
};

struct BiomeConsolidatedFeatureData {
    BiomeScatterParamData mScatter;
    int16_t mFeature = 0;
    int16_t mIdentifier = 0;
    int16_t mPass = 0;
    bool mUseInternal = false;
};

struct BiomeConsolidatedFeaturesData {
    std::vector<BiomeConsolidatedFeatureData> mFeatures;
};

struct BiomeMountainParamsData {
    uint32_t mSteepBlock = 0;
    bool mNorthSlopes = false;
    bool mSouthSlopes = false;
    bool mWestSlopes = false;
    bool mEastSlopes = false;
    bool mTopSlideEnabled = false;
};

struct BiomeSurfaceMaterialData {
    uint32_t mTopBlock = 0;
    uint32_t mMidBlock = 0;
    uint32_t mSeaFloorBlock = 0;
    uint32_t mFoundationBlock = 0;
    uint32_t mSeaBlock = 0;
    int32_t mSeaFloorDepth = 0;
};

struct BiomeElementData {
    float mNoiseFrequencyScale = 0.0f;
    float mNoiseLowerBound = 0.0f;
    float mNoiseUpperBound = 0.0f;
    int32_t mHeightMinType = 0;
    int16_t mHeightMin = 0;
    int32_t mHeightMaxType = 0;
    int16_t mHeightMax = 0;
    BiomeSurfaceMaterialData mSurfaceMaterial;
};

struct BiomeSurfaceMaterialAdjustmentData {
    std::vector<BiomeElementData> mAdjustments;
};

struct BiomeWeightedData {
    int16_t mBiome = 0;
    uint32_t mWeight = 0;
};

struct BiomeWeightedTemperatureData {
    int32_t mTemperature = 0;
    uint32_t mWeight = 0;
};

struct BiomeConditionalTransformationData {
    std::vector<BiomeWeightedData> mWeightedBiomes;
    int16_t mConditionJson = 0;
    uint32_t mMinPassingNeighbors = 0;
};

struct BiomeOverworldGenRulesData {
    std::vector<BiomeWeightedData> mHillTransformations;
    std::vector<BiomeWeightedData> mMutateTransformations;
    std::vector<BiomeWeightedData> mRiverTransformations;
    std::vector<BiomeWeightedData> mShoreTransformations;
    std::vector<BiomeConditionalTransformationData> mPreHillsEdges;
    std::vector<BiomeConditionalTransformationData> mPostShoreEdges;
    std::vector<BiomeWeightedTemperatureData> mClimates;
};

struct BiomeMultinoiseGenRulesData {
    float mTemperature = 0.0f;
    float mHumidity = 0.0f;
    float mAltitude = 0.0f;
    float mWeirdness = 0.0f;
    float mWeight = 0.0f;
};

struct BiomeLegacyWorldGenRulesData {
    std::vector<BiomeConditionalTransformationData> mLegacyPreHills;
};

struct BiomeReplacementData {
    int16_t mBiome = 0;
    int32_t mDimension = 0;
    std::vector<int16_t> mTargetBiomes;
    float mAmount = 0.0f;
    uint32_t mReplacementIndex = 0;
};

struct BiomeMesaSurfaceData {
    uint32_t mClayMaterial = 0;
    uint32_t mHardClayMaterial = 0;
    bool mBrycePillars = false;
    bool mForest = false;
};

struct BiomeCappedSurfaceData {
    std::vector<uint32_t> mFloorBlocks;
    std::vector<uint32_t> mCeilingBlocks;
    std::optional<uint32_t> mSeaBlock;
    std::optional<uint32_t> mFoundationBlock;
    std::optional<uint32_t> mBeachBlock;
};

struct BiomeNoiseBlockSpecifier {
    std::string mNoise;
    float mThreshold = 0.0f;
    float mMin = 0.0f;
    float mMax = 0.0f;
    uint32_t mBlock = 0;
};

struct BiomeNoiseGradientSurfaceData {
    std::vector<uint32_t> mNonReplaceableBlocks;
    std::vector<BiomeNoiseBlockSpecifier> mGradientBlocks;
    std::string mNoiseSeed;
    uint32_t mFirstOctave = 0;
    std::vector<float> mAmplitudes;
};

struct BiomeSurfaceBuilderData {
    std::optional<BiomeSurfaceMaterialData> mSurfaceMaterial;
    bool mDefaultOverworldSurface = false;
    bool mSwampSurface = false;
    bool mFrozenOceanSurface = false;
    bool mTheEndSurface = false;
    std::optional<BiomeMesaSurfaceData> mMesaSurface;
    std::optional<BiomeCappedSurfaceData> mCappedSurface;
    std::optional<BiomeNoiseGradientSurfaceData> mNoiseGradientSurface;
};

struct BiomeDefinitionChunkGenData {
    std::optional<BiomeClimateData> mClimate;
    std::optional<BiomeConsolidatedFeaturesData> mConsolidatedFeatures;
    std::optional<BiomeMountainParamsData> mMountainParams;
    std::optional<BiomeSurfaceMaterialAdjustmentData> mSurfaceMaterialAdjustment;
    std::optional<BiomeOverworldGenRulesData> mOverworldGenRules;
    std::optional<BiomeMultinoiseGenRulesData> mMultinoiseGenRules;
    std::optional<BiomeLegacyWorldGenRulesData> mLegacyWorldGenRules;
    std::optional<std::vector<BiomeReplacementData>> mReplacementsData;
    std::optional<uint8_t> mVillageType;
    std::optional<BiomeSurfaceBuilderData> mSurfaceBuilderData;
    std::optional<BiomeSurfaceBuilderData> mSubSurfaceBuilderData;
};
