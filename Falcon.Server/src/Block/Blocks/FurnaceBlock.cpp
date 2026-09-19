#include "Block/Blocks/FurnaceBlock.h"

#include "Level/Level.h"

#include "Inventory/Container/FurnaceContainerManagerModel.h"
#include "Inventory/InventoryManager.h"
#include "Actor/ServerPlayer.h"
#include "Block/Actor/FurnaceBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Network/Handler/ServerNetworkHandler.h"

bool FurnaceBlock::matches(const BlockState &state) {
    return matches(state.mName);
}

bool FurnaceBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:furnace" || identifier == "minecraft:lit_furnace"
           || identifier == "minecraft:blast_furnace" || identifier == "minecraft:lit_blast_furnace"
           || identifier == "minecraft:smoker" || identifier == "minecraft:lit_smoker";
}

FurnaceKind FurnaceBlock::kind(const BlockState &state) {
    if (state.mName == "minecraft:blast_furnace" || state.mName == "minecraft:lit_blast_furnace") {
        return FurnaceKind::BlastFurnace;
    }
    if (state.mName == "minecraft:smoker" || state.mName == "minecraft:lit_smoker") {
        return FurnaceKind::Smoker;
    }
    return FurnaceKind::Furnace;
}

ContainerType FurnaceBlock::containerType(const BlockState &state) {
    switch (kind(state)) {
        case FurnaceKind::BlastFurnace:
            return ContainerType::BlastFurnace;
        case FurnaceKind::Smoker:
            return ContainerType::Smoker;
        default:
            return ContainerType::Furnace;
    }
}

bool FurnaceBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state) const {
    FurnaceContainerManagerModel model(state);
    return model.open(owner, player, position);
}

void FurnaceBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                            const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;

    FurnaceBlockActor &furnace = owner.getLevelFor(player).getBlockActors().getOrCreate<FurnaceBlockActor>(position);
    furnace.mKind = kind(state);
}

void FurnaceBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const {
    (void) state;

    InventoryManager::onFurnaceBroken(owner, level, position);
}
