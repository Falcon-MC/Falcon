#pragma once

#include "Protocol/Packets/CameraAimAssistPresetsPacket.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// one camera aim assist category: targeting priorities, larger first
struct AimAssistCategory {
    std::string mIdentifier;
    std::vector<std::pair<std::string, int32_t>> mEntityPriorities;
    std::vector<std::pair<std::string, int32_t>> mBlockPriorities;
    std::vector<std::pair<std::string, int32_t>> mBlockTagPriorities;
    std::vector<std::pair<std::string, int32_t>> mEntityTypeFamilyPriorities;
    std::optional<int32_t> mDefaultEntityPriority;
    std::optional<int32_t> mDefaultBlockPriority;
};

// one camera aim assist preset: the category each held item uses and what it never targets
struct AimAssistPreset {
    std::string mIdentifier;
    std::vector<std::string> mExcludedBlocks;
    std::vector<std::string> mExcludedEntities;
    std::vector<std::string> mExcludedBlockTags;
    std::vector<std::string> mExcludedEntityTypeFamilies;
    std::vector<std::string> mLiquidTargetingItems;
    std::vector<std::pair<std::string, std::string>> mItemSettings;
    std::optional<std::string> mDefaultItemSettings;
    std::optional<std::string> mHandSettings;
};

// the world's aim assist presets and categories: the vanilla pack's (BedrockData's aim_assist.json), then the
// script ones; an identifier added again replaces its entry in place (sub_14C8A34D0, 26.60)
class AimAssistRegistry {
public:
    static constexpr const char *DEFAULT_CATEGORY = "minecraft:default";
    static constexpr const char *DEFAULT_PRESET = "minecraft:aim_assist_default";

    AimAssistRegistry();

    const AimAssistCategory *findCategory(const std::string &identifier) const;

    const AimAssistPreset *findPreset(const std::string &identifier) const;

    const std::vector<AimAssistCategory> &categories() const {
        return mCategories;
    }

    const std::vector<AimAssistPreset> &presets() const {
        return mPresets;
    }

    void addCategory(AimAssistCategory category);

    void addPreset(AimAssistPreset preset);

    // every entry, as a joining player receives them (sub_143779C10, 26.60)
    CameraAimAssistPresetsPacket makePacket() const;

    static CameraAimAssistCategory toPacket(const AimAssistCategory &category);

    static CameraAimAssistPresetDefinition toPacket(const AimAssistPreset &preset);

private:
    void _loadVanilla();

    std::vector<AimAssistCategory> mCategories;
    std::vector<AimAssistPreset> mPresets;
};
