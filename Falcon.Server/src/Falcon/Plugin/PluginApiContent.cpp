#include "Falcon/Plugin/PluginServerApi.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Falcon/Plugin/PluginContentRegistry.h"
#include "Scripting/Content/CustomContentRegistry.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace PluginApiHelpers;

namespace {
    uint32_t eventResourcePackCount(FalconEvent *handle) {
        const auto *offer = event(handle)->mResourcePackOffer;
        return offer != nullptr ? (uint32_t) (*offer)->mPacks.size() : 0;
    }

    int eventResourcePackAt(FalconEvent *handle, uint32_t index, FalconOfferedResourcePack *out) {
        const auto *offer = event(handle)->mResourcePackOffer;
        if (offer == nullptr || out == nullptr || index >= (*offer)->mPacks.size())
            return 0;
        const ResourcePack &pack = (*offer)->mPacks[index];
        *out = {pack.mUuidString.c_str(), pack.mName.c_str(), pack.mVersion.c_str(), pack.mContentKey.c_str(),
                pack.mSha256.c_str(), pack.mSize, pack.mData.data(), pack.mData.size()};
        return 1;
    }

    int eventReplaceResourcePackOffer(FalconEvent *handle, const FalconResourcePackOfferEntry *packs, uint32_t count,
                                      int required) {
        PluginEvent *source = event(handle);
        if (source->mMonitor || source->mResourcePackOffer == nullptr || count > 128 || (count != 0 && packs == nullptr))
            return 0;
        auto &offer = *source->mResourcePackOffer;
        auto replacement = ResourcePackManager::replaceOffer(*offer, packs, count, required != 0);
        if (replacement == nullptr)
            return 0;
        offer = std::move(replacement);
        source->mState = offer->mRequired;
        return 1;
    }

    int resourcePacksReplace(FalconPlugin *pluginHandle, const FalconResourcePackDescriptor *packs, uint32_t count,
                             int required) {
        constexpr uint32_t MAX_PACKS = 128;
        constexpr size_t MAX_BYTES = 128 * 1024 * 1024;
        if (pluginHandle == nullptr || count > MAX_PACKS || (count != 0 && packs == nullptr))
            return 0;

        size_t total = 0;
        std::vector<std::pair<std::string, std::string>> archives;
        archives.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const auto &pack = packs[i];
            if (pack.data == nullptr || pack.size == 0 || pack.size > MAX_BYTES - total)
                return 0;
            total += pack.size;
            archives.emplace_back(std::string(pack.data, pack.size),
                                  pack.contentKey == nullptr ? std::string() : std::string(pack.contentKey));
        }
        return owner().getResourcePacks().replacePluginPacks(plugin(pluginHandle)->mDescription.mName,
                                                           std::move(archives), required != 0) ? 1 : 0;
    }

    std::string textOr(const char *value, const std::string &fallback) {
        if (value == nullptr || value[0] == '\0')
            return fallback;
        return value;
    }

    std::string pathOf(const std::string &identifier) {
        return identifier.substr(identifier.find(':') + 1);
    }

    bool acceptIdentifier(LoadedPlugin *owner, const char *kind, const char *value, bool frozen) {
        const char *name = owner->mDescription.mName.c_str();
        if (value == nullptr || value[0] == '\0') {
            LOG_ERROR(LogAreaID::Server, "[%s] Cannot register a custom %s without an identifier", name, kind);
            return false;
        }

        const std::string identifier = value;
        const size_t colon = identifier.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= identifier.size()) {
            LOG_ERROR(LogAreaID::Server, "[%s] Cannot register custom %s %s: the identifier needs a namespace", name,
                      kind, value);
            return false;
        }

        if (identifier.compare(0, colon, "minecraft") == 0) {
            LOG_ERROR(LogAreaID::Server, "[%s] Cannot register custom %s %s: the minecraft namespace is reserved", name,
                      kind, value);
            return false;
        }

        CustomContentRegistry &content = CustomContentRegistry::getInstance();
        if (frozen) {
            LOG_ERROR(LogAreaID::Server,
                      "[%s] Cannot register custom %s %s: custom content registration has closed", name, kind,
                      value);
            return false;
        }

        if (content.hasIdentifier(identifier)) {
            LOG_ERROR(LogAreaID::Server, "[%s] Cannot register custom %s %s: the identifier is already registered",
                      name, kind, value);
            return false;
        }

        return true;
    }

    void rejectDuplicate(LoadedPlugin *owner, const char *kind, const std::string &identifier) {
        LOG_ERROR(LogAreaID::Server, "[%s] Cannot register custom %s %s: the identifier is already registered",
                  owner->mDescription.mName.c_str(), kind, identifier.c_str());
    }

    int registerCustomItemDefinition(FalconPlugin *source, const FalconCustomItemDescriptor *descriptor,
                                     const json::Value *components) {
        LoadedPlugin *owner = plugin(source);
        if (owner == nullptr || descriptor == nullptr)
            return 0;
        if (!acceptIdentifier(owner, "item", descriptor->identifier, CustomContentRegistry::getInstance().areItemsFrozen()))
            return 0;

        CustomItemDefinition definition;
        definition.mIdentifier = descriptor->identifier;
        definition.mDisplayName = textOr(descriptor->displayName, pathOf(definition.mIdentifier));
        definition.mIcon = textOr(descriptor->icon, pathOf(definition.mIdentifier));
        definition.mCreativeCategory = textOr(descriptor->creativeCategory, definition.mCreativeCategory);
        if (descriptor->maxStackSize > 0)
            definition.mMaxStackSize = descriptor->maxStackSize > 64 ? 64 : (int32_t) descriptor->maxStackSize;
        definition.mMaxDurability = (int32_t) descriptor->maxDurability;
        definition.mHandEquipped = descriptor->handEquipped != 0;
        // the descriptor has no field for it: plugin items keep going in the off hand
        definition.mAllowOffHand = true;
        CustomContentRegistry::readItemComponents(definition, components);
        definition.mMaxStackSize = std::clamp(definition.mMaxStackSize, 1, 64);

        const CustomItemDefinition *stored = CustomContentRegistry::getInstance().registerItem(
                definition, PluginApiHelpers::owner().getItemDefinitions());
        if (stored == nullptr) {
            rejectDuplicate(owner, "item", definition.mIdentifier);
            return 0;
        }

        PluginContentRegistry::ItemEntry entry;
        entry.mPlugin = owner;
        entry.mOnUse = descriptor->onUse;
        entry.mOnUseOnBlock = descriptor->onUseOnBlock;
        entry.mUserData = descriptor->userData;
        PluginContentRegistry::getInstance().addItem(
                Item(stored->mNetworkId, stored->mIdentifier, stored->mDisplayName, stored->mMaxStackSize), entry);
        return 1;
    }

    int registerCustomItem(FalconPlugin *source, const FalconCustomItemDescriptor *descriptor) {
        return registerCustomItemDefinition(source, descriptor, nullptr);
    }

    int registerCustomItemWithComponents(FalconPlugin *source, const FalconCustomItemDescriptorV2 *descriptor) {
        if (descriptor == nullptr)
            return 0;
        if (descriptor->componentsSize == 0)
            return registerCustomItem(source, &descriptor->item);
        if (descriptor->components == nullptr || descriptor->componentsSize > FALCON_CUSTOM_ITEM_COMPONENTS_MAX_BYTES)
            return 0;

        const std::string text(descriptor->components, descriptor->componentsSize);
        const auto components = json::parse(text);
        if (components == nullptr || !components->isObject())
            return 0;

        return registerCustomItemDefinition(source, &descriptor->item, components.get());
    }

    int registerCustomBlock(FalconPlugin *source, const FalconCustomBlockDescriptor *descriptor) {
        LoadedPlugin *owner = plugin(source);
        if (owner == nullptr || descriptor == nullptr)
            return 0;
        if (!acceptIdentifier(owner, "block", descriptor->identifier, CustomContentRegistry::getInstance().isFrozen()))
            return 0;

        CustomBlockDefinition definition;
        definition.mIdentifier = descriptor->identifier;
        definition.mDisplayName = textOr(descriptor->displayName, pathOf(definition.mIdentifier));
        definition.mTexture = textOr(descriptor->texture, pathOf(definition.mIdentifier));
        definition.mMenuCategory = textOr(descriptor->creativeCategory, definition.mMenuCategory);
        if (descriptor->destroyTime > 0.0f) {
            definition.mDestroyTime = descriptor->destroyTime;
            definition.mHasDestroyTime = true;
        }
        if (descriptor->explosionResistance > 0.0f) {
            definition.mExplosionResistance = descriptor->explosionResistance;
            definition.mHasExplosionResistance = true;
        }
        if (descriptor->friction > 0.0f) {
            definition.mFriction = descriptor->friction;
            definition.mHasFriction = true;
        }
        definition.mLightEmission = descriptor->lightEmission > 15 ? 15 : (int32_t) descriptor->lightEmission;

        ServerNetworkHandler &server = PluginApiHelpers::owner();
        const CustomBlockDefinition *stored = CustomContentRegistry::getInstance().registerBlock(
                definition, server.getItemDefinitions(), server.getBlockDefinitions());
        if (stored == nullptr) {
            rejectDuplicate(owner, "block", definition.mIdentifier);
            return 0;
        }

        PluginContentRegistry::BlockEntry entry;
        entry.mPlugin = owner;
        entry.mOnInteract = descriptor->onInteract;
        entry.mOnBreak = descriptor->onBreak;
        entry.mUserData = descriptor->userData;
        entry.mDrop = descriptor->drop == nullptr ? std::string() : std::string(descriptor->drop);
        PluginContentRegistry::getInstance().addBlock(
                Block(stored->mNetworkHash, stored->mIdentifier, stored->mDisplayName, Tag::ofCompound()), entry);
        return 1;
    }

    int registerCustomEntity(FalconPlugin *source, const FalconCustomEntityDescriptor *descriptor) {
        LoadedPlugin *owner = plugin(source);
        if (owner == nullptr || descriptor == nullptr)
            return 0;
        if (!acceptIdentifier(owner, "entity", descriptor->identifier, CustomContentRegistry::getInstance().isFrozen()))
            return 0;

        PluginContentRegistry::EntityEntry entry;
        entry.mPlugin = owner;
        if (descriptor->width > 0.0f)
            entry.mWidth = descriptor->width;
        if (descriptor->height > 0.0f)
            entry.mHeight = descriptor->height;
        if (descriptor->maxHealth > 0.0f)
            entry.mMaxHealth = descriptor->maxHealth;
        entry.mOnTick = descriptor->onTick;
        entry.mOnInteract = descriptor->onInteract;
        entry.mUserData = descriptor->userData;

        CustomActorDefinition definition;
        definition.mIdentifier = descriptor->identifier;
        definition.mIsSummonable = descriptor->summonable != 0;
        definition.mCollisionWidth = entry.mWidth;
        definition.mCollisionHeight = entry.mHeight;

        if (CustomContentRegistry::getInstance().registerActor(definition) == nullptr) {
            rejectDuplicate(owner, "entity", definition.mIdentifier);
            return 0;
        }

        PluginContentRegistry::getInstance().addEntity(definition.mIdentifier, entry);
        return 1;
    }
}

void PluginServerApi::fillContent(FalconServerApi &api) {
    api.registerCustomItem = &registerCustomItem;
    api.registerCustomItemWithComponents = &registerCustomItemWithComponents;
    api.registerCustomBlock = &registerCustomBlock;
    api.registerCustomEntity = &registerCustomEntity;
    api.resourcePacksReplace = &resourcePacksReplace;
    api.eventResourcePackCount = &eventResourcePackCount;
    api.eventResourcePackAt = &eventResourcePackAt;
    api.eventReplaceResourcePackOffer = &eventReplaceResourcePackOffer;
}
