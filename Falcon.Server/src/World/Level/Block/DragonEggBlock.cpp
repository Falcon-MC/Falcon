#include "World/Level/Block/DragonEggBlock.h"

#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Level/Block/BlockNames.h"

#include <cmath>
#include <cstdlib>

FALCON_REGISTER_BLOCK(DragonEggBlock, 9);

namespace {
    const int32_t PARTICLE_DRAGON_EGG_EVENT = 2010;

    int nextInt(int minimum, int maximum) {
        const int span = maximum - minimum;
        if (span <= 0)
            return minimum;

        return minimum + (std::rand() % span);
    }
}

bool DragonEggBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:dragon_egg";
}

void DragonEggBlock::onTouch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                             const BlockState &state) const {
    teleport(owner, owner.getLevelFor(player), position, state);
}

bool DragonEggBlock::onPunch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                             const BlockState &state) const {
    if (player.isCreative())
        return false;

    teleport(owner, owner.getLevelFor(player), position, state);
    return true;
}

void DragonEggBlock::teleport(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) {
    for (int attempt = 0; attempt < TELEPORT_ATTEMPTS; ++attempt) {
        Vector3i destination(position.x + nextInt(-TELEPORT_HORIZONTAL_RANGE, TELEPORT_HORIZONTAL_RANGE),
                                   position.y + nextInt(0, TELEPORT_VERTICAL_RANGE),
                                   position.z + nextInt(-TELEPORT_HORIZONTAL_RANGE, TELEPORT_HORIZONTAL_RANGE));

        if (destination.y < level.getMinY() || destination.y > level.getMaxY())
            continue;

        if (level.getBlockState(destination.x, destination.y, destination.z).mName != BlockNames::AIR)
            continue;

        if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_BLOCK_TELEPORT)) {
            PluginEvent event;
            event.mType = FALCON_EVENT_BLOCK_TELEPORT;
            event.mCancellable = true;
            event.mLevel = &level;
            event.mBlockPosition = position;
            event.mTo = Vector3f((float) destination.x, (float) destination.y, (float) destination.z);
            plugins->dispatch(event);
            if (event.mCancelled)
                return;
            if (event.mToChanged) {
                const Vector3f &to = event.mTo;
                if (!std::isfinite(to.x) || !std::isfinite(to.y) || !std::isfinite(to.z) || std::abs(to.x) > 30'000'000
                    || std::abs(to.z) > 30'000'000 || to.y < level.getMinY() || to.y > level.getMaxY())
                    return;
                destination = Vector3i((int32_t) std::floor(to.x), (int32_t) std::floor(to.y),
                                       (int32_t) std::floor(to.z));
                if (!level.isChunkResident(destination.x >> 4, destination.z >> 4)
                    || level.getBlockState(destination.x, destination.y, destination.z).mName != BlockNames::AIR)
                    return;
            }
        }

        const int32_t diffX = position.x - destination.x;
        const int32_t diffY = position.y - destination.y;
        const int32_t diffZ = position.z - destination.z;

        if (std::abs(diffX) <= 255 && std::abs(diffY) <= 255 && std::abs(diffZ) <= 255) {
            const int32_t data = (std::abs(diffX) << 16) | (std::abs(diffY) << 8) | std::abs(diffZ)
                                 | ((diffX < 0 ? 1 : 0) << 24) | ((diffY < 0 ? 1 : 0) << 25)
                                 | ((diffZ < 0 ? 1 : 0) << 26);
            owner.broadcastLevelEvent(level, PARTICLE_DRAGON_EGG_EVENT,
                                      Vector3f((float) position.x, (float) position.y, (float) position.z), data);
        }

        level.setBlock(position, BlockState(BlockNames::AIR), false);
        level.onBlockBroken(position, state);

        level.setBlock(destination, state, false);
        level.onBlockPlaced(destination, state);
        return;
    }
}
