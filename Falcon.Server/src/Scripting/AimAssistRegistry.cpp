#include "Scripting/AimAssistRegistry.h"

#include "AimAssistJson.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"

#include <memory>

namespace {
    std::vector<std::string> readNames(const json::Value *list) {
        std::vector<std::string> names;
        if (list == nullptr || !list->isArray())
            return names;
        for (const std::unique_ptr<json::Value> &name: list->mArray) {
            if (name->isString())
                names.push_back(name->mString);
        }
        return names;
    }

    std::vector<std::pair<std::string, int32_t>> readPriorities(const json::Value *object) {
        std::vector<std::pair<std::string, int32_t>> priorities;
        if (object == nullptr || !object->isObject())
            return priorities;
        for (const std::string &key: object->mKeys) {
            const json::Value *value = object->get(key);
            if (value != nullptr && value->mType == json::Value::Type::Number)
                priorities.emplace_back(key, value->integer());
        }
        return priorities;
    }

    std::optional<int32_t> readOptionalInt(const json::Value *value) {
        if (value == nullptr || value->mType != json::Value::Type::Number)
            return std::nullopt;
        return value->integer();
    }

    std::optional<std::string> readOptionalString(const json::Value *value) {
        if (value == nullptr || !value->isString())
            return std::nullopt;
        return value->mString;
    }

    // minecraft:aim_assist_categories entries
    AimAssistCategory readCategory(const json::Value &entry) {
        AimAssistCategory category;
        category.mIdentifier = entry.get("name")->mString;
        const json::Value *priorities = entry.get("priorities");
        if (priorities == nullptr || !priorities->isObject())
            return category;
        category.mEntityPriorities = readPriorities(priorities->get("entities"));
        category.mBlockPriorities = readPriorities(priorities->get("blocks"));
        category.mBlockTagPriorities = readPriorities(priorities->get("block_tags"));
        category.mEntityTypeFamilyPriorities = readPriorities(priorities->get("entity_type_families"));
        category.mDefaultEntityPriority = readOptionalInt(priorities->get("entity_default"));
        category.mDefaultBlockPriority = readOptionalInt(priorities->get("block_default"));
        return category;
    }

    // a minecraft:aim_assist_preset document
    AimAssistPreset readPreset(const json::Value &entry) {
        AimAssistPreset preset;
        preset.mIdentifier = entry.get("identifier")->mString;
        const json::Value *exclusions = entry.get("exclusion_settings");
        if (exclusions != nullptr && exclusions->isObject()) {
            preset.mExcludedBlocks = readNames(exclusions->get("blocks"));
            preset.mExcludedEntities = readNames(exclusions->get("entities"));
            preset.mExcludedBlockTags = readNames(exclusions->get("block_tags"));
            preset.mExcludedEntityTypeFamilies = readNames(exclusions->get("entity_type_families"));
        }
        preset.mLiquidTargetingItems = readNames(entry.get("liquid_targeting_list"));
        const json::Value *items = entry.get("item_settings");
        if (items != nullptr && items->isObject()) {
            for (const std::string &key: items->mKeys) {
                const json::Value *category = items->get(key);
                if (category != nullptr && category->isString())
                    preset.mItemSettings.emplace_back(key, category->mString);
            }
        }
        preset.mDefaultItemSettings = readOptionalString(entry.get("default_item_settings"));
        preset.mHandSettings = readOptionalString(entry.get("hand_settings"));
        return preset;
    }

    std::vector<CameraAimAssistPriority> toPriorities(const std::vector<std::pair<std::string, int32_t>> &entries) {
        std::vector<CameraAimAssistPriority> priorities;
        for (const auto &entry: entries) {
            CameraAimAssistPriority priority;
            priority.mName = entry.first;
            priority.mPriority = entry.second;
            priorities.push_back(priority);
        }
        return priorities;
    }
}

AimAssistRegistry::AimAssistRegistry() {
    _loadVanilla();
}

void AimAssistRegistry::_loadVanilla() {
    const std::unique_ptr<json::Value> root = json::parse(FalconAimAssistData::kAimAssistJson);
    const json::Value *presets = root == nullptr ? nullptr : root->get("presets");
    const json::Value *categories = root == nullptr ? nullptr : root->get("categories");
    if (presets == nullptr || categories == nullptr || !presets->isArray() || !categories->isArray()) {
        LOG_WARN(LogAreaID::Server, "Failed to parse embedded aim assist data");
        return;
    }
    for (const std::unique_ptr<json::Value> &entry: categories->mArray) {
        const json::Value *name = entry->get("name");
        if (name != nullptr && name->isString())
            addCategory(readCategory(*entry));
    }
    for (const std::unique_ptr<json::Value> &entry: presets->mArray) {
        const json::Value *identifier = entry->get("identifier");
        if (identifier != nullptr && identifier->isString())
            addPreset(readPreset(*entry));
    }
}

const AimAssistCategory *AimAssistRegistry::findCategory(const std::string &identifier) const {
    for (const AimAssistCategory &category: mCategories) {
        if (category.mIdentifier == identifier)
            return &category;
    }
    return nullptr;
}

const AimAssistPreset *AimAssistRegistry::findPreset(const std::string &identifier) const {
    for (const AimAssistPreset &preset: mPresets) {
        if (preset.mIdentifier == identifier)
            return &preset;
    }
    return nullptr;
}

void AimAssistRegistry::addCategory(AimAssistCategory category) {
    for (AimAssistCategory &existing: mCategories) {
        if (existing.mIdentifier == category.mIdentifier) {
            existing = std::move(category);
            return;
        }
    }
    mCategories.push_back(std::move(category));
}

void AimAssistRegistry::addPreset(AimAssistPreset preset) {
    for (AimAssistPreset &existing: mPresets) {
        if (existing.mIdentifier == preset.mIdentifier) {
            existing = std::move(preset);
            return;
        }
    }
    mPresets.push_back(std::move(preset));
}

CameraAimAssistCategory AimAssistRegistry::toPacket(const AimAssistCategory &category) {
    CameraAimAssistCategory entry;
    entry.mName = category.mIdentifier;
    entry.mActorPriorities = toPriorities(category.mEntityPriorities);
    entry.mBlockPriorities = toPriorities(category.mBlockPriorities);
    entry.mBlockTagPriorities = toPriorities(category.mBlockTagPriorities);
    entry.mActorTypeFamiliesPriorities = toPriorities(category.mEntityTypeFamilyPriorities);
    entry.mHasActorDefaultPriorities = category.mDefaultEntityPriority.has_value();
    entry.mActorDefaultPriorities = category.mDefaultEntityPriority.value_or(0);
    entry.mHasBlockDefaultPriorities = category.mDefaultBlockPriority.has_value();
    entry.mBlockDefaultPriorities = category.mDefaultBlockPriority.value_or(0);
    return entry;
}

CameraAimAssistPresetDefinition AimAssistRegistry::toPacket(const AimAssistPreset &preset) {
    CameraAimAssistPresetDefinition entry;
    entry.mIdentifier = preset.mIdentifier;
    entry.mBlockExclusionList = preset.mExcludedBlocks;
    entry.mActorExclusionList = preset.mExcludedEntities;
    entry.mBlockTagExclusionList = preset.mExcludedBlockTags;
    entry.mActorTypeFamiliesExclusionList = preset.mExcludedEntityTypeFamilies;
    entry.mLiquidTargetingList = preset.mLiquidTargetingItems;
    for (const auto &item: preset.mItemSettings) {
        CameraAimAssistItemSettings settings;
        settings.mItemId = item.first;
        settings.mCategory = item.second;
        entry.mItemSettings.push_back(settings);
    }
    entry.mHasDefaultItemSettings = preset.mDefaultItemSettings.has_value();
    entry.mDefaultItemSettings = preset.mDefaultItemSettings.value_or(std::string());
    entry.mHasHandSettings = preset.mHandSettings.has_value();
    entry.mHandSettings = preset.mHandSettings.value_or(std::string());
    return entry;
}

CameraAimAssistPresetsPacket AimAssistRegistry::makePacket() const {
    CameraAimAssistPresetsPacket packet;
    for (const AimAssistCategory &category: mCategories)
        packet.mCategoryDefinitions.push_back(toPacket(category));
    for (const AimAssistPreset &preset: mPresets)
        packet.mPresets.push_back(toPacket(preset));
    // the game sends every sync with operation 1, the full one at join too
    packet.mOperation = CameraAimAssistOperation::AddToExisting;
    return packet;
}
