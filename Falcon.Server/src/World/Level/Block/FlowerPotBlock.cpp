#include "World/Level/Block/FlowerPotBlock.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/FlowerPotBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockPickItem.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <string>
#include <unordered_set>
#include <vector>

FALCON_REGISTER_BLOCK(FlowerPotBlock, 200);

namespace {
    const char *const OPEN_EYEBLOSSOM = "minecraft:open_eyeblossom";
    const char *const CLOSED_EYEBLOSSOM = "minecraft:closed_eyeblossom";
    const char *const UPDATE_BIT = "update_bit";
    // the time of day an eyeblossom stays open, from its first tick for as many ticks
    const int64_t EYEBLOSSOM_OPENS = 12601;
    const int64_t EYEBLOSSOM_OPEN_TICKS = 10800;
    const int64_t TICKS_PER_DAY = 24000;
    // LevelEvent ParticleGenericSpawn and the ParticleType of each change
    const int32_t PARTICLE_EVENT = 2009;
    const int32_t EYEBLOSSOM_OPEN_PARTICLE = 96;
    const int32_t EYEBLOSSOM_CLOSE_PARTICLE = 97;
    const char *const OPEN_SOUND = "open";
    const char *const CLOSE_SOUND = "close";

    Vector3f centreOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }
}

bool FlowerPotBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:flower_pot";
}

bool FlowerPotBlock::isPottable(const std::string &blockIdentifier) {
    static const std::unordered_set<std::string> POTTABLE = {
            "minecraft:dandelion",          "minecraft:golden_dandelion",     "minecraft:poppy",
            "minecraft:blue_orchid",        "minecraft:allium",               "minecraft:azure_bluet",
            "minecraft:red_tulip",          "minecraft:orange_tulip",         "minecraft:white_tulip",
            "minecraft:pink_tulip",         "minecraft:oxeye_daisy",          "minecraft:cornflower",
            "minecraft:lily_of_the_valley", "minecraft:wither_rose",          "minecraft:torchflower",
            "minecraft:open_eyeblossom",    "minecraft:closed_eyeblossom",    "minecraft:oak_sapling",
            "minecraft:spruce_sapling",     "minecraft:birch_sapling",        "minecraft:jungle_sapling",
            "minecraft:acacia_sapling",     "minecraft:dark_oak_sapling",     "minecraft:cherry_sapling",
            "minecraft:pale_oak_sapling",   "minecraft:poplar_sapling",       "minecraft:mangrove_propagule",
            "minecraft:brown_mushroom",     "minecraft:red_mushroom",         "minecraft:crimson_fungus",
            "minecraft:warped_fungus",      "minecraft:crimson_roots",        "minecraft:warped_roots",
            "minecraft:azalea",             "minecraft:flowering_azalea",     "minecraft:fern",
            "minecraft:deadbush",           "minecraft:cactus",               "minecraft:bamboo"};
    return POTTABLE.count(blockIdentifier) != 0;
}

std::string FlowerPotBlock::eyeblossomFor(const Level &level) {
    if (!level.hasSkyLight())
        return std::string();
    const int64_t time = level.getTime() % TICKS_PER_DAY - EYEBLOSSOM_OPENS;
    return time >= 0 && time < EYEBLOSSOM_OPEN_TICKS ? OPEN_EYEBLOSSOM : CLOSED_EYEBLOSSOM;
}

const char *FlowerPotBlock::getBlockActorId() const {
    return FlowerPotBlockActor::BLOCK_ACTOR_ID;
}

void FlowerPotBlock::changed(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const FlowerPotBlockActor &pot, Actor *source) {
    BlockActionHandler::broadcastBlockActorData(owner, level, pot);
    BlockState state = level.getBlockState(position.x, position.y, position.z);
    state.mStates.putByte(UPDATE_BIT, state.mStates.getByte(UPDATE_BIT, 0) != 0 ? 0 : 1);
    level.setBlock(position, state, false);
    level.postGameEvent(GameEvents::BLOCK_CHANGE, centreOf(position), source);
}

// the game's interaction, with the hand that used the pot: a pottable plant fills an empty pot and is used up
// outside creative; any other item, or none, takes the plant back, into the inventory or onto the ground
bool FlowerPotBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                const BlockState &state) const {
    (void) state;
    Level &level = owner.getLevelFor(player);
    FlowerPotBlockActor &pot = level.getBlockActors().getOrCreate<FlowerPotBlockActor>(position);
    PlayerInventory &inventory = player.getInventory();
    const ItemStack held = inventory.getItemInHand();
    const std::string heldIdentifier =
            held.isAir() || held.mDefinition == nullptr ? std::string() : held.mDefinition->getIdentifier();
    const Block *plant = heldIdentifier.empty() ? nullptr : VanillaBlocks::fromIdentifier(heldIdentifier);
    const bool pottable = plant != nullptr && isPottable(heldIdentifier);

    if (pottable) {
        if (!pot.hasPlant()) {
            pot.setPlant(plant->toBlockState());
            player.consumeOneHeldItem();
            changed(owner, level, position, pot, &player);
        }
        return true;
    }

    if (!pot.hasPlant())
        return true;

    const ItemStack taken = owner.createItemStack(BlockPickItem::identifierFor(pot.getPlant().mName), 1);
    pot.clearPlant();
    if (!taken.isAir()) {
        std::vector<int> touched;
        if (!inventory.addItem(taken, touched))
            owner._throwItem(player, taken);
        for (const int slot: touched)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        InventoryHandler::sendHeldItem(owner, player);
    }
    changed(owner, level, position, pot, &player);
    return true;
}

void FlowerPotBlock::onPlayerWillDestroy(ServerNetworkHandler &owner, ServerPlayer &player, Level &level,
                                         const Vector3i &position, const BlockState &state) const {
    (void) owner;
    (void) state;
    if (!player.isCreative())
        return;
    if (FlowerPotBlockActor *pot = level.getBlockActors().find<FlowerPotBlockActor>(position))
        pot->clearPlant();
}

bool FlowerPotBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    return BlockSupport::isAttachable(below, PlacementOrientation::FACE_UP);
}

void FlowerPotBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    (void) state;
    FlowerPotBlockActor *pot = level.getBlockActors().find<FlowerPotBlockActor>(position);
    if (pot == nullptr || !pot->hasPlant())
        return;
    const std::string &current = pot->getPlant().mName;
    if (current != OPEN_EYEBLOSSOM && current != CLOSED_EYEBLOSSOM)
        return;
    const std::string wanted = eyeblossomFor(level);
    if (wanted.empty() || wanted == current)
        return;
    const Block *eyeblossom = VanillaBlocks::fromIdentifier(wanted);
    if (eyeblossom == nullptr)
        return;

    pot->setPlant(eyeblossom->toBlockState());
    changed(owner, level, position, *pot, nullptr);
    const bool closing = wanted == CLOSED_EYEBLOSSOM;
    owner.playLevelSound(level, closing ? CLOSE_SOUND : OPEN_SOUND, centreOf(position));
    owner.broadcastLevelEvent(level, PARTICLE_EVENT, centreOf(position),
                              closing ? EYEBLOSSOM_CLOSE_PARTICLE : EYEBLOSSOM_OPEN_PARTICLE);
}
