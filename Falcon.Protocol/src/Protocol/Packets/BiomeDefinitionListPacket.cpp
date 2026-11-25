#include "Protocol/Packets/BiomeDefinitionListPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    template<class Value, class Writer>
    void writeOptional(BinaryStream &stream, const std::optional<Value> &value, Writer writer) {
        stream.putOptionalPresent(value.has_value());
        if (value.has_value())
            writer(stream, *value);
    }

    template<class Value, class Reader>
    std::optional<Value> readOptional(ReadOnlyBinaryStream &stream, Reader reader) {
        if (!stream.getOptionalPresent())
            return std::nullopt;
        return reader(stream);
    }

    template<class Value, class Writer>
    void writeList(BinaryStream &stream, const std::vector<Value> &values, Writer writer) {
        stream.putArrayLength((uint32_t) values.size());
        for (const Value &value: values)
            writer(stream, value);
    }

    template<class Value, class Reader>
    std::vector<Value> readList(ReadOnlyBinaryStream &stream, Reader reader) {
        std::vector<Value> values;
        const uint32_t count = stream.getArrayLength();
        values.reserve(count);
        for (uint32_t i = 0; i < count; i++)
            values.push_back(reader(stream));
        return values;
    }

    void writeUnsignedInt(BinaryStream &stream, const uint32_t &value) {
        stream.putLInt(value);
    }

    uint32_t readUnsignedInt(ReadOnlyBinaryStream &stream) {
        return stream.getLInt();
    }

    void writeClimate(BinaryStream &stream, const BiomeClimateData &climate) {
        stream.putLFloat(climate.mTemperature);
        stream.putLFloat(climate.mDownfall);
        stream.putLFloat(climate.mSnowAccumulationMin);
        stream.putLFloat(climate.mSnowAccumulationMax);
    }

    BiomeClimateData readClimate(ReadOnlyBinaryStream &stream) {
        BiomeClimateData climate;
        climate.mTemperature = stream.getLFloat();
        climate.mDownfall = stream.getLFloat();
        climate.mSnowAccumulationMin = stream.getLFloat();
        climate.mSnowAccumulationMax = stream.getLFloat();
        return climate;
    }

    void writeCoordinate(BinaryStream &stream, const BiomeCoordinateData &coordinate) {
        stream.putVarInt(coordinate.mMinValueType);
        stream.putLShort((uint16_t) coordinate.mMinValue);
        stream.putVarInt(coordinate.mMaxValueType);
        stream.putLShort((uint16_t) coordinate.mMaxValue);
        stream.putLInt(coordinate.mGridOffset);
        stream.putLInt(coordinate.mGridStepSize);
        stream.putVarInt(coordinate.mDistribution);
    }

    BiomeCoordinateData readCoordinate(ReadOnlyBinaryStream &stream) {
        BiomeCoordinateData coordinate;
        coordinate.mMinValueType = stream.getVarInt();
        coordinate.mMinValue = stream.getSignedLShort();
        coordinate.mMaxValueType = stream.getVarInt();
        coordinate.mMaxValue = stream.getSignedLShort();
        coordinate.mGridOffset = stream.getLInt();
        coordinate.mGridStepSize = stream.getLInt();
        coordinate.mDistribution = stream.getVarInt();
        return coordinate;
    }

    void writeScatter(BinaryStream &stream, const BiomeScatterParamData &scatter) {
        writeList(stream, scatter.mCoordinates, writeCoordinate);
        stream.putVarInt(scatter.mEvalOrder);
        stream.putVarInt(scatter.mChancePercentType);
        stream.putLShort((uint16_t) scatter.mChancePercent);
        stream.putLInt((uint32_t) scatter.mChanceNumerator);
        stream.putLInt((uint32_t) scatter.mChanceDenominator);
        stream.putVarInt(scatter.mIterationsType);
        stream.putLShort((uint16_t) scatter.mIterations);
    }

    BiomeScatterParamData readScatter(ReadOnlyBinaryStream &stream) {
        BiomeScatterParamData scatter;
        scatter.mCoordinates = readList<BiomeCoordinateData>(stream, readCoordinate);
        scatter.mEvalOrder = stream.getVarInt();
        scatter.mChancePercentType = stream.getVarInt();
        scatter.mChancePercent = stream.getSignedLShort();
        scatter.mChanceNumerator = stream.getSignedLInt();
        scatter.mChanceDenominator = stream.getSignedLInt();
        scatter.mIterationsType = stream.getVarInt();
        scatter.mIterations = stream.getSignedLShort();
        return scatter;
    }

    void writeConsolidatedFeature(BinaryStream &stream, const BiomeConsolidatedFeatureData &feature) {
        writeScatter(stream, feature.mScatter);
        stream.putLShort((uint16_t) feature.mFeature);
        stream.putLShort((uint16_t) feature.mIdentifier);
        stream.putLShort((uint16_t) feature.mPass);
        stream.putBool(feature.mUseInternal);
    }

    BiomeConsolidatedFeatureData readConsolidatedFeature(ReadOnlyBinaryStream &stream) {
        BiomeConsolidatedFeatureData feature;
        feature.mScatter = readScatter(stream);
        feature.mFeature = stream.getSignedLShort();
        feature.mIdentifier = stream.getSignedLShort();
        feature.mPass = stream.getSignedLShort();
        feature.mUseInternal = stream.getBool();
        return feature;
    }

    void writeConsolidatedFeatures(BinaryStream &stream, const BiomeConsolidatedFeaturesData &features) {
        writeList(stream, features.mFeatures, writeConsolidatedFeature);
    }

    BiomeConsolidatedFeaturesData readConsolidatedFeatures(ReadOnlyBinaryStream &stream) {
        BiomeConsolidatedFeaturesData features;
        features.mFeatures = readList<BiomeConsolidatedFeatureData>(stream, readConsolidatedFeature);
        return features;
    }

    void writeMountainParams(BinaryStream &stream, const BiomeMountainParamsData &params) {
        stream.putLInt(params.mSteepBlock);
        stream.putBool(params.mNorthSlopes);
        stream.putBool(params.mSouthSlopes);
        stream.putBool(params.mWestSlopes);
        stream.putBool(params.mEastSlopes);
        stream.putBool(params.mTopSlideEnabled);
    }

    BiomeMountainParamsData readMountainParams(ReadOnlyBinaryStream &stream) {
        BiomeMountainParamsData params;
        params.mSteepBlock = stream.getLInt();
        params.mNorthSlopes = stream.getBool();
        params.mSouthSlopes = stream.getBool();
        params.mWestSlopes = stream.getBool();
        params.mEastSlopes = stream.getBool();
        params.mTopSlideEnabled = stream.getBool();
        return params;
    }

    void writeSurfaceMaterial(BinaryStream &stream, const BiomeSurfaceMaterialData &material) {
        stream.putLInt(material.mTopBlock);
        stream.putLInt(material.mMidBlock);
        stream.putLInt(material.mSeaFloorBlock);
        stream.putLInt(material.mFoundationBlock);
        stream.putLInt(material.mSeaBlock);
        stream.putLInt((uint32_t) material.mSeaFloorDepth);
    }

    BiomeSurfaceMaterialData readSurfaceMaterial(ReadOnlyBinaryStream &stream) {
        BiomeSurfaceMaterialData material;
        material.mTopBlock = stream.getLInt();
        material.mMidBlock = stream.getLInt();
        material.mSeaFloorBlock = stream.getLInt();
        material.mFoundationBlock = stream.getLInt();
        material.mSeaBlock = stream.getLInt();
        material.mSeaFloorDepth = stream.getSignedLInt();
        return material;
    }

    void writeElement(BinaryStream &stream, const BiomeElementData &element) {
        stream.putLFloat(element.mNoiseFrequencyScale);
        stream.putLFloat(element.mNoiseLowerBound);
        stream.putLFloat(element.mNoiseUpperBound);
        stream.putVarInt(element.mHeightMinType);
        stream.putLShort((uint16_t) element.mHeightMin);
        stream.putVarInt(element.mHeightMaxType);
        stream.putLShort((uint16_t) element.mHeightMax);
        writeSurfaceMaterial(stream, element.mSurfaceMaterial);
    }

    BiomeElementData readElement(ReadOnlyBinaryStream &stream) {
        BiomeElementData element;
        element.mNoiseFrequencyScale = stream.getLFloat();
        element.mNoiseLowerBound = stream.getLFloat();
        element.mNoiseUpperBound = stream.getLFloat();
        element.mHeightMinType = stream.getVarInt();
        element.mHeightMin = stream.getSignedLShort();
        element.mHeightMaxType = stream.getVarInt();
        element.mHeightMax = stream.getSignedLShort();
        element.mSurfaceMaterial = readSurfaceMaterial(stream);
        return element;
    }

    void writeSurfaceMaterialAdjustment(BinaryStream &stream, const BiomeSurfaceMaterialAdjustmentData &adjustment) {
        writeList(stream, adjustment.mAdjustments, writeElement);
    }

    BiomeSurfaceMaterialAdjustmentData readSurfaceMaterialAdjustment(ReadOnlyBinaryStream &stream) {
        BiomeSurfaceMaterialAdjustmentData adjustment;
        adjustment.mAdjustments = readList<BiomeElementData>(stream, readElement);
        return adjustment;
    }

    void writeWeighted(BinaryStream &stream, const BiomeWeightedData &weighted) {
        stream.putLShort((uint16_t) weighted.mBiome);
        stream.putLInt(weighted.mWeight);
    }

    BiomeWeightedData readWeighted(ReadOnlyBinaryStream &stream) {
        BiomeWeightedData weighted;
        weighted.mBiome = stream.getSignedLShort();
        weighted.mWeight = stream.getLInt();
        return weighted;
    }

    void writeWeightedTemperature(BinaryStream &stream, const BiomeWeightedTemperatureData &weighted) {
        stream.putVarInt(weighted.mTemperature);
        stream.putLInt(weighted.mWeight);
    }

    BiomeWeightedTemperatureData readWeightedTemperature(ReadOnlyBinaryStream &stream) {
        BiomeWeightedTemperatureData weighted;
        weighted.mTemperature = stream.getVarInt();
        weighted.mWeight = stream.getLInt();
        return weighted;
    }

    void writeConditionalTransformation(BinaryStream &stream, const BiomeConditionalTransformationData &data) {
        writeList(stream, data.mWeightedBiomes, writeWeighted);
        stream.putLShort((uint16_t) data.mConditionJson);
        stream.putLInt(data.mMinPassingNeighbors);
    }

    BiomeConditionalTransformationData readConditionalTransformation(ReadOnlyBinaryStream &stream) {
        BiomeConditionalTransformationData data;
        data.mWeightedBiomes = readList<BiomeWeightedData>(stream, readWeighted);
        data.mConditionJson = stream.getSignedLShort();
        data.mMinPassingNeighbors = stream.getLInt();
        return data;
    }

    void writeOverworldGenRules(BinaryStream &stream, const BiomeOverworldGenRulesData &rules) {
        writeList(stream, rules.mHillTransformations, writeWeighted);
        writeList(stream, rules.mMutateTransformations, writeWeighted);
        writeList(stream, rules.mRiverTransformations, writeWeighted);
        writeList(stream, rules.mShoreTransformations, writeWeighted);
        writeList(stream, rules.mPreHillsEdges, writeConditionalTransformation);
        writeList(stream, rules.mPostShoreEdges, writeConditionalTransformation);
        writeList(stream, rules.mClimates, writeWeightedTemperature);
    }

    BiomeOverworldGenRulesData readOverworldGenRules(ReadOnlyBinaryStream &stream) {
        BiomeOverworldGenRulesData rules;
        rules.mHillTransformations = readList<BiomeWeightedData>(stream, readWeighted);
        rules.mMutateTransformations = readList<BiomeWeightedData>(stream, readWeighted);
        rules.mRiverTransformations = readList<BiomeWeightedData>(stream, readWeighted);
        rules.mShoreTransformations = readList<BiomeWeightedData>(stream, readWeighted);
        rules.mPreHillsEdges = readList<BiomeConditionalTransformationData>(stream, readConditionalTransformation);
        rules.mPostShoreEdges = readList<BiomeConditionalTransformationData>(stream, readConditionalTransformation);
        rules.mClimates = readList<BiomeWeightedTemperatureData>(stream, readWeightedTemperature);
        return rules;
    }

    void writeMultinoiseGenRules(BinaryStream &stream, const BiomeMultinoiseGenRulesData &rules) {
        stream.putLFloat(rules.mTemperature);
        stream.putLFloat(rules.mHumidity);
        stream.putLFloat(rules.mAltitude);
        stream.putLFloat(rules.mWeirdness);
        stream.putLFloat(rules.mWeight);
    }

    BiomeMultinoiseGenRulesData readMultinoiseGenRules(ReadOnlyBinaryStream &stream) {
        BiomeMultinoiseGenRulesData rules;
        rules.mTemperature = stream.getLFloat();
        rules.mHumidity = stream.getLFloat();
        rules.mAltitude = stream.getLFloat();
        rules.mWeirdness = stream.getLFloat();
        rules.mWeight = stream.getLFloat();
        return rules;
    }

    void writeLegacyWorldGenRules(BinaryStream &stream, const BiomeLegacyWorldGenRulesData &rules) {
        writeList(stream, rules.mLegacyPreHills, writeConditionalTransformation);
    }

    BiomeLegacyWorldGenRulesData readLegacyWorldGenRules(ReadOnlyBinaryStream &stream) {
        BiomeLegacyWorldGenRulesData rules;
        rules.mLegacyPreHills = readList<BiomeConditionalTransformationData>(stream, readConditionalTransformation);
        return rules;
    }

    void writeReplacement(BinaryStream &stream, const BiomeReplacementData &replacement) {
        stream.putLShort((uint16_t) replacement.mBiome);
        stream.putVarInt(replacement.mDimension);
        stream.putArrayLength((uint32_t) replacement.mTargetBiomes.size());
        for (int16_t target: replacement.mTargetBiomes)
            stream.putLShort((uint16_t) target);
        stream.putLFloat(replacement.mAmount);
        stream.putLInt(replacement.mReplacementIndex);
    }

    BiomeReplacementData readReplacement(ReadOnlyBinaryStream &stream) {
        BiomeReplacementData replacement;
        replacement.mBiome = stream.getSignedLShort();
        replacement.mDimension = stream.getVarInt();
        const uint32_t count = stream.getArrayLength();
        for (uint32_t i = 0; i < count; i++)
            replacement.mTargetBiomes.push_back(stream.getSignedLShort());
        replacement.mAmount = stream.getLFloat();
        replacement.mReplacementIndex = stream.getLInt();
        return replacement;
    }

    void writeReplacements(BinaryStream &stream, const std::vector<BiomeReplacementData> &replacements) {
        writeList(stream, replacements, writeReplacement);
    }

    std::vector<BiomeReplacementData> readReplacements(ReadOnlyBinaryStream &stream) {
        return readList<BiomeReplacementData>(stream, readReplacement);
    }

    void writeMesaSurface(BinaryStream &stream, const BiomeMesaSurfaceData &mesa) {
        stream.putLInt(mesa.mClayMaterial);
        stream.putLInt(mesa.mHardClayMaterial);
        stream.putBool(mesa.mBrycePillars);
        stream.putBool(mesa.mForest);
    }

    BiomeMesaSurfaceData readMesaSurface(ReadOnlyBinaryStream &stream) {
        BiomeMesaSurfaceData mesa;
        mesa.mClayMaterial = stream.getLInt();
        mesa.mHardClayMaterial = stream.getLInt();
        mesa.mBrycePillars = stream.getBool();
        mesa.mForest = stream.getBool();
        return mesa;
    }

    void writeCappedSurface(BinaryStream &stream, const BiomeCappedSurfaceData &capped) {
        writeList(stream, capped.mFloorBlocks, writeUnsignedInt);
        writeList(stream, capped.mCeilingBlocks, writeUnsignedInt);
        writeOptional(stream, capped.mSeaBlock, writeUnsignedInt);
        writeOptional(stream, capped.mFoundationBlock, writeUnsignedInt);
        writeOptional(stream, capped.mBeachBlock, writeUnsignedInt);
    }

    BiomeCappedSurfaceData readCappedSurface(ReadOnlyBinaryStream &stream) {
        BiomeCappedSurfaceData capped;
        capped.mFloorBlocks = readList<uint32_t>(stream, readUnsignedInt);
        capped.mCeilingBlocks = readList<uint32_t>(stream, readUnsignedInt);
        capped.mSeaBlock = readOptional<uint32_t>(stream, readUnsignedInt);
        capped.mFoundationBlock = readOptional<uint32_t>(stream, readUnsignedInt);
        capped.mBeachBlock = readOptional<uint32_t>(stream, readUnsignedInt);
        return capped;
    }

    void writeNoiseBlockSpecifier(BinaryStream &stream, const BiomeNoiseBlockSpecifier &specifier) {
        stream.putString(specifier.mNoise);
        stream.putLFloat(specifier.mThreshold);
        stream.putLFloat(specifier.mMin);
        stream.putLFloat(specifier.mMax);
        stream.putLInt(specifier.mBlock);
    }

    BiomeNoiseBlockSpecifier readNoiseBlockSpecifier(ReadOnlyBinaryStream &stream) {
        BiomeNoiseBlockSpecifier specifier;
        specifier.mNoise = stream.getString();
        specifier.mThreshold = stream.getLFloat();
        specifier.mMin = stream.getLFloat();
        specifier.mMax = stream.getLFloat();
        specifier.mBlock = stream.getLInt();
        return specifier;
    }

    void writeNoiseGradientSurface(BinaryStream &stream, const BiomeNoiseGradientSurfaceData &gradient) {
        writeList(stream, gradient.mNonReplaceableBlocks, writeUnsignedInt);
        writeList(stream, gradient.mGradientBlocks, writeNoiseBlockSpecifier);
        stream.putString(gradient.mNoiseSeed);
        stream.putLInt(gradient.mFirstOctave);
        stream.putArrayLength((uint32_t) gradient.mAmplitudes.size());
        for (float amplitude: gradient.mAmplitudes)
            stream.putLFloat(amplitude);
    }

    BiomeNoiseGradientSurfaceData readNoiseGradientSurface(ReadOnlyBinaryStream &stream) {
        BiomeNoiseGradientSurfaceData gradient;
        gradient.mNonReplaceableBlocks = readList<uint32_t>(stream, readUnsignedInt);
        gradient.mGradientBlocks = readList<BiomeNoiseBlockSpecifier>(stream, readNoiseBlockSpecifier);
        gradient.mNoiseSeed = stream.getString();
        gradient.mFirstOctave = stream.getLInt();
        const uint32_t count = stream.getArrayLength();
        for (uint32_t i = 0; i < count; i++)
            gradient.mAmplitudes.push_back(stream.getLFloat());
        return gradient;
    }

    void writeSurfaceBuilder(BinaryStream &stream, const BiomeSurfaceBuilderData &builder) {
        writeOptional(stream, builder.mSurfaceMaterial, writeSurfaceMaterial);
        stream.putBool(builder.mDefaultOverworldSurface);
        stream.putBool(builder.mSwampSurface);
        stream.putBool(builder.mFrozenOceanSurface);
        stream.putBool(builder.mTheEndSurface);
        writeOptional(stream, builder.mMesaSurface, writeMesaSurface);
        writeOptional(stream, builder.mCappedSurface, writeCappedSurface);
        writeOptional(stream, builder.mNoiseGradientSurface, writeNoiseGradientSurface);
    }

    BiomeSurfaceBuilderData readSurfaceBuilder(ReadOnlyBinaryStream &stream) {
        BiomeSurfaceBuilderData builder;
        builder.mSurfaceMaterial = readOptional<BiomeSurfaceMaterialData>(stream, readSurfaceMaterial);
        builder.mDefaultOverworldSurface = stream.getBool();
        builder.mSwampSurface = stream.getBool();
        builder.mFrozenOceanSurface = stream.getBool();
        builder.mTheEndSurface = stream.getBool();
        builder.mMesaSurface = readOptional<BiomeMesaSurfaceData>(stream, readMesaSurface);
        builder.mCappedSurface = readOptional<BiomeCappedSurfaceData>(stream, readCappedSurface);
        builder.mNoiseGradientSurface = readOptional<BiomeNoiseGradientSurfaceData>(stream, readNoiseGradientSurface);
        return builder;
    }

    void writeVillageType(BinaryStream &stream, const uint8_t &villageType) {
        stream.putByte(villageType);
    }

    uint8_t readVillageType(ReadOnlyBinaryStream &stream) {
        return stream.getByte();
    }

    void writeChunkGenData(BinaryStream &stream, const BiomeDefinitionChunkGenData &data) {
        writeOptional(stream, data.mClimate, writeClimate);
        writeOptional(stream, data.mConsolidatedFeatures, writeConsolidatedFeatures);
        writeOptional(stream, data.mMountainParams, writeMountainParams);
        writeOptional(stream, data.mSurfaceMaterialAdjustment, writeSurfaceMaterialAdjustment);
        writeOptional(stream, data.mOverworldGenRules, writeOverworldGenRules);
        writeOptional(stream, data.mMultinoiseGenRules, writeMultinoiseGenRules);
        writeOptional(stream, data.mLegacyWorldGenRules, writeLegacyWorldGenRules);
        writeOptional(stream, data.mReplacementsData, writeReplacements);
        writeOptional(stream, data.mVillageType, writeVillageType);
        writeOptional(stream, data.mSurfaceBuilderData, writeSurfaceBuilder);
        writeOptional(stream, data.mSubSurfaceBuilderData, writeSurfaceBuilder);
    }

    BiomeDefinitionChunkGenData readChunkGenData(ReadOnlyBinaryStream &stream) {
        BiomeDefinitionChunkGenData data;
        data.mClimate = readOptional<BiomeClimateData>(stream, readClimate);
        data.mConsolidatedFeatures = readOptional<BiomeConsolidatedFeaturesData>(stream, readConsolidatedFeatures);
        data.mMountainParams = readOptional<BiomeMountainParamsData>(stream, readMountainParams);
        data.mSurfaceMaterialAdjustment =
                readOptional<BiomeSurfaceMaterialAdjustmentData>(stream, readSurfaceMaterialAdjustment);
        data.mOverworldGenRules = readOptional<BiomeOverworldGenRulesData>(stream, readOverworldGenRules);
        data.mMultinoiseGenRules = readOptional<BiomeMultinoiseGenRulesData>(stream, readMultinoiseGenRules);
        data.mLegacyWorldGenRules = readOptional<BiomeLegacyWorldGenRulesData>(stream, readLegacyWorldGenRules);
        data.mReplacementsData = readOptional<std::vector<BiomeReplacementData>>(stream, readReplacements);
        data.mVillageType = readOptional<uint8_t>(stream, readVillageType);
        data.mSurfaceBuilderData = readOptional<BiomeSurfaceBuilderData>(stream, readSurfaceBuilder);
        data.mSubSurfaceBuilderData = readOptional<BiomeSurfaceBuilderData>(stream, readSurfaceBuilder);
        return data;
    }

    void writeTagIndexes(BinaryStream &stream, const std::vector<uint16_t> &tagIndexes) {
        stream.putArrayLength((uint32_t) tagIndexes.size());
        for (uint16_t index: tagIndexes)
            stream.putLShort(index);
    }

    std::vector<uint16_t> readTagIndexes(ReadOnlyBinaryStream &stream) {
        std::vector<uint16_t> tagIndexes;
        const uint32_t count = stream.getArrayLength();
        for (uint32_t i = 0; i < count; i++)
            tagIndexes.push_back(stream.getLShort());
        return tagIndexes;
    }

    void writeDefinition(BinaryStream &stream, const BiomeDefinitionData &biome) {
        stream.putLShort(biome.mId);

        stream.putLFloat(biome.mTemperature);
        stream.putLFloat(biome.mDownfall);
        stream.putLFloat(biome.mFoliageSnow);
        stream.putLFloat(biome.mDepth);
        stream.putLFloat(biome.mScale);
        stream.putLInt((uint32_t) biome.mMapWaterColorArgb);
        stream.putBool(biome.mRain);

        writeOptional(stream, biome.mTagIndexes, writeTagIndexes);
        writeOptional(stream, biome.mChunkGenData, writeChunkGenData);
    }

}

BiomeDefinitionListPacket::BiomeDefinitionListPacket() = default;

void BiomeDefinitionListPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mBiomes.size());
    for (uint32_t i = 0; i < (uint32_t) mBiomes.size(); i++) {
        stream.putLShort(mStrings.empty() ? (uint16_t) i : mBiomes[i].mNameIndex);
        writeDefinition(stream, mBiomes[i]);
    }

    if (!mStrings.empty()) {
        stream.putArrayLength((uint32_t) mStrings.size());
        for (const std::string &value: mStrings)
            stream.putString(value);
        return;
    }

    stream.putArrayLength((uint32_t) mBiomes.size());
    for (const BiomeDefinitionData &biome: mBiomes)
        stream.putString(biome.mName);
}

void BiomeDefinitionListPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mBiomes.clear();
    mStrings.clear();

    const uint32_t biomeCount = stream.getArrayLength();
    mBiomes.reserve(biomeCount);

    for (uint32_t i = 0; i < biomeCount; i++) {
        BiomeDefinitionData biome;
        biome.mNameIndex = stream.getLShort();
        biome.mId = stream.getLShort();
        biome.mTemperature = stream.getLFloat();
        biome.mDownfall = stream.getLFloat();
        biome.mFoliageSnow = stream.getLFloat();
        biome.mDepth = stream.getLFloat();
        biome.mScale = stream.getLFloat();
        biome.mMapWaterColorArgb = (int32_t) stream.getLInt();
        biome.mRain = stream.getBool();
        biome.mTagIndexes = readOptional<std::vector<uint16_t>>(stream, readTagIndexes);
        biome.mChunkGenData = readOptional<BiomeDefinitionChunkGenData>(stream, readChunkGenData);
        mBiomes.push_back(std::move(biome));
    }

    const uint32_t stringCount = stream.getArrayLength();
    mStrings.reserve(stringCount);
    for (uint32_t i = 0; i < stringCount; i++)
        mStrings.push_back(stream.getString());

    for (BiomeDefinitionData &biome: mBiomes) {
        if (biome.mNameIndex < mStrings.size())
            biome.mName = mStrings[biome.mNameIndex];
    }
}

void BiomeDefinitionListPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
