#include "Item/Items/BoneMealItem.h"

#include "Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(BoneMealItem, 100);

#include "Block/BlockIdentifier.h"
#include "Block/Blocks/CropBlock.h"
#include "Block/Blocks/NetherWartBlock.h"
#include "Block/Blocks/NyliumBlock.h"
#include "Block/Blocks/SaplingBlock.h"
#include "Block/Blocks/StemBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Overworld/Feature/Tree/LegacyTallGrass.h"
#include "Level/Generator/Random/SimpleRandom.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <chrono>
#include <string>
#include <vector>

namespace {
    const int32_t BONE_MEAL_USE_EVENT = 2005;

    int64_t randomSeed() {
        return (int64_t) std::chrono::steady_clock::now().time_since_epoch().count();
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    const char *growthStateOf(const BlockState &state) {
        return state.mName == "minecraft:nether_wart" ? "age" : "growth";
    }

    int maxGrowthOf(const BlockState &state) {
        return state.mName == "minecraft:nether_wart" ? 3 : 7;
    }

    bool isCrop(const BlockState &state) {
        return CropBlock::matches(state.mName) || StemBlock::matches(state.mName)
               || NetherWartBlock::matches(state.mName);
    }

    class TrackingBlockManager : public BlockManager {
    public:
        explicit TrackingBlockManager(Level &level) : BlockManager(level), mLevel(level) {}

        void setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) override {
            if (!BlockChangeSystem::allows(mLevel, Vector3i(x, y, z), state, BlockChangeCause::Grow))
                return;

            BlockManager::setBlockStateAt(x, y, z, state);
            mChanged.push_back(Vector3i(x, y, z));
        }

        const std::vector<Vector3i> &getChanged() const { return mChanged; }

    private:
        Level &mLevel;
        std::vector<Vector3i> mChanged;
    };

    bool applyManager(ServerNetworkHandler &owner, Level &level, TrackingBlockManager &manager) {
        if (manager.getChanged().empty())
            return false;

        const std::vector<Vector3i> changed = manager.getChanged();
        manager.applySubChunkUpdate();

        for (const Vector3i &position: changed) {
            const BlockState state = level.getBlockState(position.x, position.y, position.z);
            BlockActionHandler::broadcastBlockUpdate(owner, level, position, state);
        }

        return true;
    }
}

BoneMealItem::BoneMealItem(const Item &base) : Item(base) {}

bool BoneMealItem::matches(const std::string &identifier) {
    return identifier == "minecraft:bone_meal";
}

bool BoneMealItem::applyToCrop(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) {
    const char *growthState = growthStateOf(state);
    const int maxGrowth = maxGrowthOf(state);
    const int32_t growth = state.mStates.getInt(growthState);

    if (growth >= maxGrowth)
        return false;

    int32_t grown = growth + 2 + RandomTickSystem::nextInt(3);
    if (grown > maxGrowth)
        grown = maxGrowth;

    Tag states = state.mStates;
    states.putInt(growthState, grown);
    return BlockChangeSystem::change(level, position, BlockState(state.mName, states), BlockChangeCause::Grow,
                                     false);
}

bool BoneMealItem::applyToSapling(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) {
    if (state.mStates.getByte("age_bit") == 0) {
        Tag states = state.mStates;
        states.putByte("age_bit", 1);
        return BlockChangeSystem::change(level, position, BlockState(state.mName, states), BlockChangeCause::Grow,
                                         false);
    }

    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    if (block == nullptr)
        return false;

    block->onRandomTick(owner, level, position, state);
    return level.getBlockState(position.x, position.y, position.z).mName != state.mName;
}

bool BoneMealItem::applyToGrass(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    if (level.getBlockState(position.x, position.y + 1, position.z).mName != "minecraft:air")
        return false;

    TrackingBlockManager manager(level);
    SimpleRandom random(randomSeed());
    LegacyTallGrass::growGrass(manager, position.x, position.y, position.z, random);

    return applyManager(owner, level, manager);
}

bool BoneMealItem::applyToNylium(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state) {
    const Vector3i above(position.x, position.y + 1, position.z);
    if (level.getBlockState(above.x, above.y, above.z).mName != "minecraft:air")
        return false;

    const bool crimson = state.mName == "minecraft:crimson_nylium";
    const int roll = RandomTickSystem::nextInt(10);

    std::string grown;
    if (roll == 0)
        grown = crimson ? "minecraft:crimson_fungus" : "minecraft:warped_fungus";
    else
        grown = crimson ? "minecraft:crimson_roots" : "minecraft:warped_roots";

    return BlockChangeSystem::change(level, above, BlockState(grown), BlockChangeCause::Grow, false);
}

bool BoneMealItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                const Vector3i &blockPosition, int32_t face,
                                const Vector3f &clickPosition) const {
    (void) face;
    (void) clickPosition;

    if (player.getGameType() == (int32_t) GameType::Spectator)
        return false;

    Level &level = owner.getLevelFor(player);
    const BlockState state = level.getBlockState(blockPosition.x, blockPosition.y, blockPosition.z);

    bool applied = false;
    if (isCrop(state))
        applied = applyToCrop(owner, level, blockPosition, state);
    else if (SaplingBlock::matches(state.mName))
        applied = applyToSapling(owner, level, blockPosition, state);
    else if (state.mName == "minecraft:grass_block")
        applied = applyToGrass(owner, level, blockPosition);
    else if (NyliumBlock::matches(state.mName))
        applied = applyToNylium(owner, level, blockPosition, state);

    if (!applied)
        return false;

    player.startItemCooldown(item, owner.getCurrentTick(), USE_COOLDOWN_TICKS);

    owner.broadcastLevelEvent(level, BONE_MEAL_USE_EVENT, centerOf(blockPosition), 0);

    if (player.getGameType() != (int32_t) GameType::Creative) {
        ItemStack remaining = item;
        remaining.mCount--;
        if (remaining.mCount <= 0)
            remaining = ItemStack::air();

        PlayerInventory &inventory = player.getInventory();
        inventory.setItemInHand(std::move(remaining));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
    }

    return true;
}
