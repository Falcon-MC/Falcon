#include "Plugin/PluginItem.h"

#include "Actor/ServerPlayer.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Item/ItemClassRegistry.h"
#include "Plugin/PluginApiHelpers.h"
#include "Plugin/PluginContentRegistry.h"
#include "Protocol/Types/ItemStack.h"

FALCON_REGISTER_ITEM(PluginItem, 0);

namespace {
    const PluginContentRegistry::ItemEntry *activeEntry(const std::string &identifier) {
        const PluginContentRegistry::ItemEntry *entry = PluginContentRegistry::getInstance().findItem(identifier);
        if (entry == nullptr || entry->mPlugin == nullptr || !entry->mPlugin->mEnabled)
            return nullptr;
        return entry;
    }
}

PluginItem::PluginItem(const Item &base) : Item(base) {}

bool PluginItem::matches(const std::string &identifier) {
    return PluginContentRegistry::getInstance().findItem(identifier) != nullptr;
}

bool PluginItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) owner;

    const PluginContentRegistry::ItemEntry *entry = activeEntry(getIdentifier());
    if (entry == nullptr || entry->mOnUse == nullptr)
        return false;

    ItemStack used = item;
    try {
        return entry->mOnUse(PluginApiHelpers::toHandle(&player), PluginApiHelpers::toHandle(&used),
                             entry->mUserData) != 0;
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] onUse of %s threw an exception",
                  entry->mPlugin->mDescription.mName.c_str(), getIdentifier().c_str());
        return false;
    }
}

bool PluginItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                              const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) owner;
    (void) clickPosition;

    const PluginContentRegistry::ItemEntry *entry = activeEntry(getIdentifier());
    if (entry == nullptr || entry->mOnUseOnBlock == nullptr)
        return false;

    ItemStack used = item;
    const FalconBlockPos position{blockPosition.x, blockPosition.y, blockPosition.z};
    try {
        return entry->mOnUseOnBlock(PluginApiHelpers::toHandle(&player), PluginApiHelpers::toHandle(&used), position,
                                    (uint32_t) face, entry->mUserData) != 0;
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] onUseOnBlock of %s threw an exception",
                  entry->mPlugin->mDescription.mName.c_str(), getIdentifier().c_str());
        return false;
    }
}
