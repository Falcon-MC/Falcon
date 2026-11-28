#include "World/Level/Block/StructureBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/StructureBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/Systems/StructureBlockSystem.h"

FALCON_REGISTER_BLOCK(StructureBlock, 30);

bool StructureBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::STRUCTURE_BLOCK;
}

const char *StructureBlock::getBlockActorId() const {
    return StructureBlockActor::BLOCK_ACTOR_ID;
}

BlockState StructureBlock::applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const {
    BlockState result = state;
    result.mStates.putString("structure_block_type", "save");
    return result;
}

void StructureBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    owner.getLevelFor(player).getBlockActors().getOrCreate<StructureBlockActor>(position);
}

bool StructureBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                const BlockState &state) const {
    if (!StructureBlockSystem::canEdit(player, position))
        return true;

    auto &actor = owner.getLevelFor(player).getBlockActors().getOrCreate<StructureBlockActor>(position);
    BlockActorDataPacket packet;
    packet.mBlockPosition = position;
    packet.mData = actor.getSpawnCompound();
    owner.sendPacketTo(player.getNetworkIdentifier(), packet);
    player.getInventoryManager().openContainer(ContainerType::StructureEditor, position);
    return true;
}

void StructureBlock::onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) const {
    auto &actor = level.getBlockActors().getOrCreate<StructureBlockActor>(position);
    const bool powered = RedstoneSystem::getStrength(level, position) > 0;
    const bool rising = powered && !actor.mPowered;
    actor.mPowered = powered;
    if (rising)
        owner.getWorldOf(level).getStructureBlocks().activate(owner, level, actor, actor.mRedstoneSaveMode == 1);
}
