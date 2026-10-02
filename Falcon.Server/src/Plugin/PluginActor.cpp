#include "Plugin/PluginActor.h"

#include "Actor/ServerPlayer.h"
#include "Core/Debug/BedrockLog.h"
#include "Plugin/PluginApiHelpers.h"
#include "Plugin/PluginContentRegistry.h"

namespace {
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
    MobActor::tick(owner);

    const PluginContentRegistry::EntityEntry *entry = activeEntry(getTypeId());
    if (entry == nullptr || entry->mOnTick == nullptr)
        return;

    try {
        entry->mOnTick(PluginApiHelpers::toHandle(static_cast<Actor *>(this)), entry->mUserData);
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] onTick of %s threw an exception",
                  entry->mPlugin->mDescription.mName.c_str(), getTypeId().c_str());
    }
}

bool PluginActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    (void) owner;

    const PluginContentRegistry::EntityEntry *entry = activeEntry(getTypeId());
    if (entry == nullptr || entry->mOnInteract == nullptr)
        return false;

    try {
        return entry->mOnInteract(PluginApiHelpers::toHandle(static_cast<Actor *>(this)),
                                  PluginApiHelpers::toHandle(&player), entry->mUserData) != 0;
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] onInteract of %s threw an exception",
                  entry->mPlugin->mDescription.mName.c_str(), getTypeId().c_str());
        return false;
    }
}
