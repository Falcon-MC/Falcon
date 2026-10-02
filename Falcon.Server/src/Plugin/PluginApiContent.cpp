#include "Plugin/PluginServerApi.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginApiHelpers.h"
#include "Plugin/PluginContentRegistry.h"
#include "Scripting/Content/CustomContentRegistry.h"

#include <string>

using namespace PluginApiHelpers;

namespace {
    std::string textOr(const char *value, const std::string &fallback) {
        if (value == nullptr || value[0] == '\0')
            return fallback;
        return value;
    }

    std::string pathOf(const std::string &identifier) {
        return identifier.substr(identifier.find(':') + 1);
    }

    bool acceptIdentifier(LoadedPlugin *owner, const char *kind, const char *value) {
        const char *name = owner->mDescription.mName.c_str();
        if (value == nullptr || value[0] == '\0') {
            LOG_ERROR(LogAreaID::Server, "[%s] Cannot register a custom %s without an identifier", name, kind);
            return false;
        }

        const std::string identifier = value;
        const size_t colon = identifier.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= identifier.size()) {
            LOG_ERROR(LogAreaID::Server, "[%s] Cannot register custom %s %s: the identifier needs a namespace",
                      name, kind, value);
            return false;
        }

        if (identifier.compare(0, colon, "minecraft") == 0) {
            LOG_ERROR(LogAreaID::Server, "[%s] Cannot register custom %s %s: the minecraft namespace is reserved",
                      name, kind, value);
            return false;
        }

        CustomContentRegistry &content = CustomContentRegistry::getInstance();
        if (content.isFrozen()) {
            LOG_ERROR(LogAreaID::Server,
                      "[%s] Cannot register custom %s %s: custom content must be registered in onLoad", name, kind,
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

    int registerCustomItem(FalconPlugin *source, const FalconCustomItemDescriptor *descriptor) {
        LoadedPlugin *owner = plugin(source);
        if (owner == nullptr || descriptor == nullptr)
            return 0;
        if (!acceptIdentifier(owner, "item", descriptor->identifier))
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

    int registerCustomBlock(FalconPlugin *source, const FalconCustomBlockDescriptor *descriptor) {
        LoadedPlugin *owner = plugin(source);
        if (owner == nullptr || descriptor == nullptr)
            return 0;
        if (!acceptIdentifier(owner, "block", descriptor->identifier))
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
        if (!acceptIdentifier(owner, "entity", descriptor->identifier))
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
    api.registerCustomBlock = &registerCustomBlock;
    api.registerCustomEntity = &registerCustomEntity;
}
