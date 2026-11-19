#include "World/Level/Block/SkullBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Mob/Hostile/WitherActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/SkullBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/DispenseSource.h"
#include "World/Level/Level.h"

#include <cmath>

FALCON_REGISTER_BLOCK(WitherSkeletonSkullBlock, 204);
FALCON_REGISTER_BLOCK(SkullBlock, 205);

bool SkullBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier,
                                      {"minecraft:creeper_head", "minecraft:dragon_head", "minecraft:piglin_head",
                                       "minecraft:player_head", "minecraft:skeleton_skull",
                                       "minecraft:wither_skeleton_skull", "minecraft:zombie_head"});
}

PistonMoveReaction SkullBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

const char *SkullBlock::getBlockActorId() const {
    return SkullBlockActor::BLOCK_ACTOR_ID;
}

void SkullBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                          const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    const float rotation = blockFace == PlacementOrientation::FACE_UP
                                   ? SkullBlockActor::floorRotation(player.getRotation().y)
                                   : 0.0f;
    SkullBlockActor::place(owner, owner.getLevelFor(player), position, state, rotation);
}

bool WitherSkeletonSkullBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::WITHER_SKELETON_SKULL;
}

void WitherSkeletonSkullBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                        const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    SkullBlock::onPlaced(owner, player, position, state, usedItem, blockFace);
    WitherActor::trySummon(owner, owner.getLevelFor(player), position);
}

bool WitherSkeletonSkullBlock::dispense(const DispenseSource &source) const {
    const Vector3i target((int32_t) std::floor(source.mPosition.x), (int32_t) std::floor(source.mPosition.y),
                          (int32_t) std::floor(source.mPosition.z));
    if (!WitherActor::canDispenseSkull(source.mLevel, target))
        return false;

    const BlockState state = toBlockState();
    source.mLevel.setBlock(target, state, true);
    // the game stands a dispensed skull at 180 degrees, whatever way the dispenser faces
    SkullBlockActor::place(source.mOwner, source.mLevel, target, state, 180.0f);
    WitherActor::trySummon(source.mOwner, source.mLevel, target);
    source.playClick();
    source.removeOne();
    return true;
}
