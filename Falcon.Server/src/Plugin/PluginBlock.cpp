#include "Plugin/PluginBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Math/Vector3i.h"
#include "Plugin/PluginApiHelpers.h"
#include "Plugin/PluginContentRegistry.h"

FALCON_REGISTER_BLOCK(PluginBlock, 0);

namespace {
    const PluginContentRegistry::BlockEntry *activeEntry(const std::string &identifier) {
        const PluginContentRegistry::BlockEntry *entry = PluginContentRegistry::getInstance().findBlock(identifier);
        if (entry == nullptr || entry->mPlugin == nullptr || !entry->mPlugin->mEnabled)
            return nullptr;
        return entry;
    }

    FalconBlockPos toBlockPos(const Vector3i &position) {
        return FalconBlockPos{position.x, position.y, position.z};
    }
}

PluginBlock::PluginBlock(const Block &base) : Block(base) {}

bool PluginBlock::matches(const std::string &identifier) {
    return PluginContentRegistry::getInstance().findBlock(identifier) != nullptr;
}

bool PluginBlock::interactAt(ServerPlayer &player, const Vector3i &position, uint32_t face,
                             const std::string &identifier) {
    const PluginContentRegistry::BlockEntry *entry = activeEntry(identifier);
    if (entry == nullptr || entry->mOnInteract == nullptr)
        return false;

    try {
        return entry->mOnInteract(PluginApiHelpers::toHandle(&player), toBlockPos(position), face,
                                  entry->mUserData) != 0;
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] onInteract of %s threw an exception",
                  entry->mPlugin->mDescription.mName.c_str(), identifier.c_str());
        return false;
    }
}

bool PluginBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                           std::vector<BlockDrop> &drops) const {
    (void) tool;
    (void) fortuneLevel;

    const PluginContentRegistry::BlockEntry *entry = PluginContentRegistry::getInstance().findBlock(state.mName);
    if (entry == nullptr)
        return false;

    drops.push_back(BlockDrop{entry->mDrop.empty() ? state.mName : entry->mDrop, 1});
    return true;
}

void PluginBlock::brokenBy(ServerPlayer &player, const Vector3i &position, const std::string &identifier) {
    const PluginContentRegistry::BlockEntry *entry = activeEntry(identifier);
    if (entry == nullptr || entry->mOnBreak == nullptr)
        return;

    try {
        entry->mOnBreak(PluginApiHelpers::toHandle(&player), toBlockPos(position), entry->mUserData);
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] onBreak of %s threw an exception",
                  entry->mPlugin->mDescription.mName.c_str(), identifier.c_str());
    }
}
