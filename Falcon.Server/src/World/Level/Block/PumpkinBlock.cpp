#include "World/Level/Block/PumpkinBlock.h"

#include "Core/Math/VectorMath.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

FALCON_REGISTER_BLOCK(PumpkinBlock, 200);

namespace {
    const char *const PUMPKIN = "minecraft:pumpkin";
    const char *const CARVED_PUMPKIN = "minecraft:carved_pumpkin";
    const char *const SEEDS = "minecraft:pumpkin_seeds";
    const char *const CARDINAL_DIRECTION = "minecraft:cardinal_direction";
    const char *const DIRECTION_NAMES[] = {"south", "west", "north", "east"};
    // the game's table from a clicked face to a cardinal direction; -1 on up and down
    const int32_t FACE_TO_DIRECTION[] = {-1, -1, 2, 0, 1, 3};
}

bool PumpkinBlock::matches(const std::string &identifier) {
    return identifier == PUMPKIN;
}

const char *PumpkinBlock::carvedDirection(int32_t face) {
    const int32_t direction = face >= 0 && face <= 5 ? FACE_TO_DIRECTION[face] : -1;
    return DIRECTION_NAMES[direction < 0 ? 0 : direction];
}

void PumpkinBlock::carve(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                         int32_t face) const {
    Level &level = owner.getLevelFor(player);
    Tag states = Tag::ofCompound();
    states.putString(CARDINAL_DIRECTION, carvedDirection(face));
    const BlockState carved(CARVED_PUMPKIN, states);
    level.setBlock(position, carved, true);
    // a carved pumpkin set on a golem body builds it, as when placed
    level.onBlockPlaced(position, carved);

    if (level.getGameRules().getBool("dotiledrops"))
        owner.dropItem(level, ItemActorHandler::randomPopPosition(position), owner.createItemStack(SEEDS, 1),
                       ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);

    owner.damagePlayerHeldItem(player, 1);
    owner.playLevelSound(level, "pumpkin.carve", VectorMath::centerOf(position));
    level.postGameEvent(GameEvents::SHEAR, VectorMath::centerOf(position), &player);
}
