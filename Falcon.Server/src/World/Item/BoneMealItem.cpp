#include "World/Item/BoneMealItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(BoneMealItem, 100);

#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/CropBlock.h"
#include "World/Level/Block/NetherWartBlock.h"
#include "World/Level/Block/NyliumBlock.h"
#include "World/Level/Block/SaplingBlock.h"
#include "World/Level/Block/StemBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyTallGrass.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Core/Math/VectorMath.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/DispenseSource.h"

#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace {
    const int32_t BONE_MEAL_USE_EVENT = 2005;
    const char *const SOUND_DISPENSE_FAIL = "block.click.fail";
    // the extra data the game sends with the dispenser's failed bone meal click
    const int32_t FAIL_SOUND_DATA = 1000;

    int64_t randomSeed() {
        return (int64_t) std::chrono::steady_clock::now().time_since_epoch().count();
    }

    using VectorMath::centerOf;

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

        const std::vector<Vector3i> &getChanged() const {
            return mChanged;
        }

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
    return BlockChangeSystem::change(level, position, BlockState(state.mName, states), BlockChangeCause::Grow, false);
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
    if (level.getBlockState(position.x, position.y + 1, position.z).mName != BlockNames::AIR)
        return false;

    TrackingBlockManager manager(level);
    SimpleRandom random(randomSeed());
    LegacyTallGrass::growGrass(manager, position.x, position.y, position.z, random);

    return applyManager(owner, level, manager);
}

bool BoneMealItem::applyToNylium(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state) {
    const Vector3i above(position.x, position.y + 1, position.z);
    if (level.getBlockState(above.x, above.y, above.z).mName != BlockNames::AIR)
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

bool BoneMealItem::fertilize(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    if (isCrop(state))
        return applyToCrop(owner, level, position, state);
    if (SaplingBlock::matches(state.mName))
        return applyToSapling(owner, level, position, state);
    if (state.mName == BlockNames::GRASS_BLOCK)
        return applyToGrass(owner, level, position);
    if (NyliumBlock::matches(state.mName))
        return applyToNylium(owner, level, position, state);
    return false;
}

bool BoneMealItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) face;
    (void) clickPosition;

    if (player.isSpectator())
        return false;

    Level &level = owner.getLevelFor(player);
    if (!fertilize(owner, level, blockPosition))
        return false;

    player.startItemCooldown(item, owner.getCurrentTick(), USE_COOLDOWN_TICKS);

    owner.broadcastLevelEvent(level, BONE_MEAL_USE_EVENT, centerOf(blockPosition), 0);

    player.consumeOneHeldItem();

    return true;
}

// BoneMealItem::dispense: the block the dispense position is in; a block that takes
// no bone meal keeps the item with block.click.fail, and the dispense still counts as done
bool BoneMealItem::dispense(const DispenseSource &source) const {
    const Vector3i target((int32_t) std::floor(source.mPosition.x), (int32_t) std::floor(source.mPosition.y),
                          (int32_t) std::floor(source.mPosition.z));
    if (!fertilize(source.mOwner, source.mLevel, target)) {
        source.mOwner.playLevelSound(source.mLevel, SOUND_DISPENSE_FAIL, source.mPosition, ":", FAIL_SOUND_DATA);
        return true;
    }

    source.playClick();
    source.mOwner.broadcastLevelEvent(source.mLevel, BONE_MEAL_USE_EVENT,
                                      Vector3f((float) target.x, (float) target.y, (float) target.z), 0);
    source.removeOne();
    return true;
}
