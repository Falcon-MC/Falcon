#include "Block/Blocks/DragonEggBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

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
    if (player.getGameType() == (int32_t) GameType::Creative)
        return false;

    teleport(owner, owner.getLevelFor(player), position, state);
    return true;
}

void DragonEggBlock::teleport(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) {
    for (int attempt = 0; attempt < TELEPORT_ATTEMPTS; ++attempt) {
        const Vector3i destination(position.x + nextInt(-TELEPORT_HORIZONTAL_RANGE, TELEPORT_HORIZONTAL_RANGE),
                                   position.y + nextInt(0, TELEPORT_VERTICAL_RANGE),
                                   position.z + nextInt(-TELEPORT_HORIZONTAL_RANGE, TELEPORT_HORIZONTAL_RANGE));

        if (destination.y < level.getMinY() || destination.y > level.getMaxY())
            continue;

        if (level.getBlockState(destination.x, destination.y, destination.z).mName != "minecraft:air")
            continue;

        const int32_t diffX = position.x - destination.x;
        const int32_t diffY = position.y - destination.y;
        const int32_t diffZ = position.z - destination.z;

        const int32_t data = (std::abs(diffX) << 16) | (std::abs(diffY) << 8) | std::abs(diffZ)
                             | ((diffX < 0 ? 1 : 0) << 24) | ((diffY < 0 ? 1 : 0) << 25)
                             | ((diffZ < 0 ? 1 : 0) << 26);
        owner.broadcastLevelEvent(level, PARTICLE_DRAGON_EGG_EVENT,
                                  Vector3f((float) position.x, (float) position.y, (float) position.z), data);

        level.setBlock(position, BlockState("minecraft:air"), false);
        level.onBlockBroken(position, state);

        level.setBlock(destination, state, false);
        level.onBlockPlaced(destination, state);
        return;
    }
}
