#include "World/Level/Block/CrafterBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Inventory/CraftingManager.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/DispenserBlock.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"

#include <vector>

// before ContainerBlock, which matches the crafter too
FALCON_REGISTER_BLOCK(CrafterBlock, 45);

namespace {
    const char *const STATE_ORIENTATION = "orientation";
    const char *const STATE_TRIGGERED_BIT = "triggered_bit";
    const char *const STATE_CRAFTING = "crafting";
    const char *const SOUND_CRAFT = "crafter.craft";
    const char *const SOUND_FAIL = "crafter.fail";
    const int32_t LEVEL_EVENT_PARTICLE_CRAFTER = 3609;

    // the facing of each orientation, down/up/north/south/west/east as 0..5 (asc_14F3E5070)
    struct Orientation {
        const char *mName;
        int mFacing;
    };

    const Orientation ORIENTATIONS[] = {
            {"down_east", 0}, {"down_north", 0}, {"down_south", 0}, {"down_west", 0},
            {"up_east", 1},   {"up_north", 1},   {"up_south", 1},   {"up_west", 1},
            {"west_up", 4},   {"east_up", 5},    {"north_up", 2},   {"south_up", 3}};

    const int FACE_OFFSETS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

    // an unknown orientation reads as south_up, as the game clamps it to the last one
    int facingOf(const BlockState &state) {
        const std::string orientation = state.mStates.getString(STATE_ORIENTATION);
        for (const Orientation &entry: ORIENTATIONS) {
            if (orientation == entry.mName)
                return entry.mFacing;
        }
        return 3;
    }

    // every output of the recipe: some, like the cake, give back their buckets as extra outputs
    std::vector<ItemStack> recipeOutputs(ServerNetworkHandler &owner, int32_t networkId) {
        std::vector<ItemStack> results;
        const std::vector<uint32_t> &sources = owner.getRecipeSourceIndices();
        if (networkId <= 0 || (size_t) networkId > sources.size())
            return results;

        const size_t source = sources[(size_t) networkId - 1];
        if (source >= CraftingRecipeTable::getRecipeCount()) {
            const std::vector<CustomRecipe> &custom = CustomContentRegistry::getInstance().getRecipes();
            const size_t index = source - CraftingRecipeTable::getRecipeCount();
            if (index < custom.size()) {
                ItemStack result = owner.createItemStack(custom[index].mResultItem, custom[index].mResultCount);
                if (!result.isAir())
                    results.push_back(std::move(result));
            }
            return results;
        }

        const CraftingRecipeData &recipe = CraftingRecipeTable::getRecipes()[source];
        const CraftingOutputData *outputs = CraftingRecipeTable::getOutputs();
        for (uint32_t i = 0; i < recipe.mOutputCount; ++i) {
            const CraftingOutputData &output = outputs[recipe.mOutputOffset + i];
            ItemStack result = owner.createItemStack(output.mItemId, output.mCount);
            if (!result.isAir())
                results.push_back(std::move(result));
        }
        return results;
    }

    // sub_144C21270: whatever fits goes one item at a time into the container in front
    bool insertInFront(ServerNetworkHandler &owner, Level &level, const Vector3i &front,
                       std::vector<ItemStack> &results) {
        BlockActor *target = level.getBlockActors().find(front);
        Container *container = target != nullptr ? target->getContainer() : nullptr;
        if (container == nullptr)
            return false;

        bool inserted = false;
        for (ItemStack &result: results) {
            while (result.mCount > 0 && HopperBlockActor::addOne(*container, result)) {
                --result.mCount;
                inserted = true;
            }
        }
        if (inserted)
            owner.refreshContainerViewers(front);

        std::vector<ItemStack> remaining;
        for (ItemStack &result: results) {
            if (result.mCount > 0)
                remaining.push_back(std::move(result));
        }
        results = std::move(remaining);
        return inserted;
    }
}

bool CrafterBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:crafter";
}

void CrafterBlock::onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state) {
    const bool powered = RedstoneSystem::isGettingPower(owner, level, position);
    const bool triggered = state.mStates.getByte(STATE_TRIGGERED_BIT, 0) != 0;
    if (powered == triggered)
        return;

    Tag states = state.mStates;
    states.putByte(STATE_TRIGGERED_BIT, powered ? 1 : 0);
    level.setBlock(position, BlockState(state.mName, states), false);
    if (powered)
        level.scheduleUpdate(position, TRIGGER_DELAY_TICKS);
}

void CrafterBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) const {
    CrafterBlockActor *crafter = level.getBlockActors().find<CrafterBlockActor>(position);
    if (crafter == nullptr)
        return;

    SimpleContainerInventory &inventory = crafter->getInventory();
    std::vector<ItemStack> grid;
    for (int slot = 0; slot < CrafterBlockActor::SIZE; ++slot)
        grid.push_back(inventory.getContainerItem(slot));

    const Vector3f corner((float) position.x, (float) position.y, (float) position.z);
    const int32_t networkId =
            CraftingManager::findNetworkRecipe(grid, 3, owner.getRecipeOutputs(), owner.getRecipeSourceIndices());
    std::vector<ItemStack> results = recipeOutputs(owner, networkId);
    if (results.empty()) {
        owner.playLevelSound(level, SOUND_FAIL, corner);
        return;
    }

    const int facing = facingOf(state);
    const Vector3i front(position.x + FACE_OFFSETS[facing][0], position.y + FACE_OFFSETS[facing][1],
                         position.z + FACE_OFFSETS[facing][2]);
    insertInFront(owner, level, front, results);

    if (!results.empty()) {
        for (const ItemStack &result: results)
            DispenserBlock::ejectItem(owner, level, position, facing, result);

        const Vector3f ejected((float) position.x + 0.5f + 0.7f * (float) FACE_OFFSETS[facing][0],
                               (float) position.y + 0.3f + 0.7f * (float) FACE_OFFSETS[facing][1],
                               (float) position.z + 0.5f + 0.7f * (float) FACE_OFFSETS[facing][2]);
        owner.broadcastLevelEvent(level, LEVEL_EVENT_PARTICLE_CRAFTER, ejected,
                                  FACE_OFFSETS[facing][0] + 3 * FACE_OFFSETS[facing][2] + 4);
        owner.playLevelSound(level, SOUND_CRAFT, corner);
    }

    // one of every filled slot, whatever the recipe asked for
    for (int slot = 0; slot < CrafterBlockActor::SIZE; ++slot) {
        ItemStack item = inventory.getContainerItem(slot);
        if (item.isAir() || item.mCount <= 0)
            continue;
        --item.mCount;
        inventory.setContainerItem(slot, item.mCount <= 0 ? ItemStack::air() : std::move(item));
    }
    owner.refreshContainerViewers(position);

    crafter->mCraftingTicks = CrafterBlockActor::CRAFTING_TICKS;
    if (state.mStates.getByte(STATE_CRAFTING, 0) == 0) {
        Tag states = state.mStates;
        states.putByte(STATE_CRAFTING, 1);
        level.setBlock(position, BlockState(state.mName, states), false);
    }
    RedstoneSystem::queueRedstoneNotification(level, position);
}

int CrafterBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    const CrafterBlockActor *crafter = level.getBlockActors().find<CrafterBlockActor>(position);
    return crafter == nullptr ? 0 : crafter->getComparatorSignal();
}
