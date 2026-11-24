#include "Falcon/Plugin/PluginActor.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "Core/Debug/BedrockLog.h"
#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Falcon/Plugin/PluginContentRegistry.h"

namespace {
    // the save keeps what the plugin set, so a reloaded entity is never shown before its plugin finds it again
    const char *const TAG_PLUGIN_DRAWN = "PluginDrawn";
    const char *const TAG_PLUGIN_WIDTH = "PluginWidth";
    const char *const TAG_PLUGIN_HEIGHT = "PluginHeight";

    const PluginContentRegistry::EntityEntry *activeEntry(const std::string &identifier) {
        const PluginContentRegistry::EntityEntry *entry = PluginContentRegistry::getInstance().findEntity(identifier);
        if (entry == nullptr || entry->mPlugin == nullptr || !entry->mPlugin->mEnabled)
            return nullptr;
        return entry;
    }
}

PluginActor::PluginActor(uint64_t runtimeId, const std::string &identifier) : MobActor(runtimeId, identifier) {}

ActorCategory PluginActor::getCategory() const {
    return ActorCategory::Other;
}

ActorSize PluginActor::getSize() const {
    if (mWidth > 0.0f && mHeight > 0.0f)
        return ActorSize{mWidth, mHeight};
    const PluginContentRegistry::EntityEntry *entry = PluginContentRegistry::getInstance().findEntity(getTypeId());
    if (entry == nullptr)
        return ActorSize{DEFAULT_WIDTH, DEFAULT_HEIGHT};
    return ActorSize{entry->mWidth, entry->mHeight};
}

float PluginActor::getDefaultMaxHealth() const {
    const PluginContentRegistry::EntityEntry *entry = PluginContentRegistry::getInstance().findEntity(getTypeId());
    if (entry == nullptr)
        return 20.0f;
    return entry->mMaxHealth;
}

void PluginActor::tick(ServerNetworkHandler &owner) {
    // the plugin moves and burns the one it draws: Falcon's physics, AI and fire would only fight it
    if (!mPluginDrawn)
        MobActor::tick(owner);

    const PluginContentRegistry::EntityEntry *entry = activeEntry(getTypeId());
    if (entry == nullptr || entry->mOnTick == nullptr)
        return;

    try {
        entry->mOnTick(PluginApiHelpers::toHandle(static_cast<Actor *>(this)), entry->mUserData);
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] onTick of %s threw an exception", entry->mPlugin->mDescription.mName.c_str(),
                  getTypeId().c_str());
    }
}

bool PluginActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    (void) owner;

    const PluginContentRegistry::EntityEntry *entry = activeEntry(getTypeId());
    if (entry == nullptr || entry->mOnInteract == nullptr)
        return false;

    try {
        return entry->mOnInteract(PluginApiHelpers::toHandle(static_cast<Actor *>(this)),
                                  PluginApiHelpers::toHandle(&player), entry->mUserData)
               != 0;
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] onInteract of %s threw an exception",
                  entry->mPlugin->mDescription.mName.c_str(), getTypeId().c_str());
        return false;
    }
}

Tag PluginActor::saveNbt() const {
    Tag data = MobActor::saveNbt();
    if (mPluginDrawn)
        data.putByte(TAG_PLUGIN_DRAWN, 1);
    if (mWidth > 0.0f && mHeight > 0.0f) {
        data.putFloat(TAG_PLUGIN_WIDTH, mWidth);
        data.putFloat(TAG_PLUGIN_HEIGHT, mHeight);
    }
    return data;
}

void PluginActor::loadNbt(const Tag &data) {
    MobActor::loadNbt(data);
    mPluginDrawn = data.getByte(TAG_PLUGIN_DRAWN, 0) != 0;
    mWidth = data.getFloat(TAG_PLUGIN_WIDTH, 0.0f);
    mHeight = data.getFloat(TAG_PLUGIN_HEIGHT, 0.0f);
}
