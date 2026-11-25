#include "Protocol/Packets/CameraAimAssistPresetsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    void writePriority(BinaryStream &stream, const CameraAimAssistPriority &priority) {
        stream.putString(priority.mName);
        stream.putLInt((uint32_t) priority.mPriority);
    }

    CameraAimAssistPriority readPriority(ReadOnlyBinaryStream &stream) {
        CameraAimAssistPriority priority;
        priority.mName = stream.getString();
        priority.mPriority = (int32_t) stream.getLInt();
        return priority;
    }

    void writePriorityArray(BinaryStream &stream, const std::vector<CameraAimAssistPriority> &priorities) {
        stream.putArrayLength((uint32_t) priorities.size());
        for (const CameraAimAssistPriority &priority: priorities) {
            writePriority(stream, priority);
        }
    }

    void readPriorityArray(ReadOnlyBinaryStream &stream, std::vector<CameraAimAssistPriority> &priorities) {
        uint32_t length = stream.getArrayLength();
        priorities.reserve(length);
        for (uint32_t i = 0; i < length; i++) {
            priorities.push_back(readPriority(stream));
        }
    }

    void writeCategory(BinaryStream &stream, const CameraAimAssistCategory &category) {
        stream.putString(category.mName);
        writePriorityArray(stream, category.mActorPriorities);
        writePriorityArray(stream, category.mBlockPriorities);
        writePriorityArray(stream, category.mBlockTagPriorities);
        writePriorityArray(stream, category.mActorTypeFamiliesPriorities);

        stream.putOptionalPresent(category.mHasActorDefaultPriorities);
        if (category.mHasActorDefaultPriorities) {
            stream.putLInt((uint32_t) category.mActorDefaultPriorities);
        }

        stream.putOptionalPresent(category.mHasBlockDefaultPriorities);
        if (category.mHasBlockDefaultPriorities) {
            stream.putLInt((uint32_t) category.mBlockDefaultPriorities);
        }
    }

    CameraAimAssistCategory readCategory(ReadOnlyBinaryStream &stream) {
        CameraAimAssistCategory category;
        category.mName = stream.getString();
        readPriorityArray(stream, category.mActorPriorities);
        readPriorityArray(stream, category.mBlockPriorities);
        readPriorityArray(stream, category.mBlockTagPriorities);
        readPriorityArray(stream, category.mActorTypeFamiliesPriorities);

        category.mHasActorDefaultPriorities = stream.getOptionalPresent();
        if (category.mHasActorDefaultPriorities) {
            category.mActorDefaultPriorities = (int32_t) stream.getLInt();
        }

        category.mHasBlockDefaultPriorities = stream.getOptionalPresent();
        if (category.mHasBlockDefaultPriorities) {
            category.mBlockDefaultPriorities = (int32_t) stream.getLInt();
        }

        return category;
    }

    void writeStringArray(BinaryStream &stream, const std::vector<std::string> &values) {
        stream.putArrayLength((uint32_t) values.size());
        for (const std::string &value: values) {
            stream.putString(value);
        }
    }

    void readStringArray(ReadOnlyBinaryStream &stream, std::vector<std::string> &values) {
        uint32_t length = stream.getArrayLength();
        values.reserve(length);
        for (uint32_t i = 0; i < length; i++) {
            values.push_back(stream.getString());
        }
    }

    void writeItemSetting(BinaryStream &stream, const CameraAimAssistItemSettings &settings) {
        stream.putString(settings.mItemId);
        stream.putString(settings.mCategory);
    }

    CameraAimAssistItemSettings readItemSetting(ReadOnlyBinaryStream &stream) {
        CameraAimAssistItemSettings settings;
        settings.mItemId = stream.getString();
        settings.mCategory = stream.getString();
        return settings;
    }

    void writePreset(BinaryStream &stream, const CameraAimAssistPresetDefinition &preset) {
        stream.putString(preset.mIdentifier);
        writeStringArray(stream, preset.mBlockExclusionList);
        writeStringArray(stream, preset.mActorExclusionList);
        writeStringArray(stream, preset.mBlockTagExclusionList);
        writeStringArray(stream, preset.mActorTypeFamiliesExclusionList);
        writeStringArray(stream, preset.mLiquidTargetingList);

        stream.putArrayLength((uint32_t) preset.mItemSettings.size());
        for (const CameraAimAssistItemSettings &settings: preset.mItemSettings) {
            writeItemSetting(stream, settings);
        }

        stream.putOptionalPresent(preset.mHasDefaultItemSettings);
        if (preset.mHasDefaultItemSettings) {
            stream.putString(preset.mDefaultItemSettings);
        }

        stream.putOptionalPresent(preset.mHasHandSettings);
        if (preset.mHasHandSettings) {
            stream.putString(preset.mHandSettings);
        }
    }

    CameraAimAssistPresetDefinition readPreset(ReadOnlyBinaryStream &stream) {
        CameraAimAssistPresetDefinition preset;
        preset.mIdentifier = stream.getString();
        readStringArray(stream, preset.mBlockExclusionList);
        readStringArray(stream, preset.mActorExclusionList);
        readStringArray(stream, preset.mBlockTagExclusionList);
        readStringArray(stream, preset.mActorTypeFamiliesExclusionList);
        readStringArray(stream, preset.mLiquidTargetingList);

        uint32_t itemSettingsLength = stream.getArrayLength();
        preset.mItemSettings.reserve(itemSettingsLength);
        for (uint32_t i = 0; i < itemSettingsLength; i++) {
            preset.mItemSettings.push_back(readItemSetting(stream));
        }

        preset.mHasDefaultItemSettings = stream.getOptionalPresent();
        if (preset.mHasDefaultItemSettings) {
            preset.mDefaultItemSettings = stream.getString();
        }

        preset.mHasHandSettings = stream.getOptionalPresent();
        if (preset.mHasHandSettings) {
            preset.mHandSettings = stream.getString();
        }

        return preset;
    }

}

CameraAimAssistPresetsPacket::CameraAimAssistPresetsPacket() = default;

void CameraAimAssistPresetsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mCategoryDefinitions.size());
    for (const CameraAimAssistCategory &category: mCategoryDefinitions) {
        writeCategory(stream, category);
    }

    stream.putArrayLength((uint32_t) mPresets.size());
    for (const CameraAimAssistPresetDefinition &preset: mPresets) {
        writePreset(stream, preset);
    }

    stream.putByte((unsigned char) mOperation);
}

void CameraAimAssistPresetsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t categoryCount = stream.getArrayLength();
    mCategoryDefinitions.reserve(categoryCount);
    for (uint32_t i = 0; i < categoryCount; i++) {
        mCategoryDefinitions.push_back(readCategory(stream));
    }

    uint32_t presetCount = stream.getArrayLength();
    mPresets.reserve(presetCount);
    for (uint32_t i = 0; i < presetCount; i++) {
        mPresets.push_back(readPreset(stream));
    }

    mOperation = (CameraAimAssistOperation) stream.getByte();
}

void CameraAimAssistPresetsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
