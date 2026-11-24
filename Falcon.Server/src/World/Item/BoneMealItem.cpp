#include "World/Item/BoneMealItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(BoneMealItem, 100);

#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/CropBlock.h"
#include "World/Level/Block/NetherWartBlock.h"
#include "World/Level/Block/NyliumBlock.h"
#include "World/Level/Block/BigDripleafBlock.h"
#include "World/Level/Block/SaplingBlock.h"
#include "World/Level/Block/SmallDripleafBlock.h"
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
        TrackingBlockManager(Level &level, ServerPlayer *player) : BlockManager(level), mLevel(level), mPlayer(player) {}

        void setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) override {
            if (!BlockChangeSystem::allows(mLevel, Vector3i(x, y, z), state, BlockChangeCause::Grow, nullptr, mPlayer))
                return;

            BlockManager::setBlockStateAt(x, y, z, state);
            mChanged.push_back(Vector3i(x, y, z));
        }

        const std::vector<Vector3i> &getChanged() const {
            return mChanged;
        }

    private:
        Level &mLevel;
        ServerPlayer *mPlayer;
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
                               const BlockState &state, ServerPlayer *player) {
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
    return BlockChangeSystem::change(level, position, BlockState(state.mName, states), BlockChangeCause::Grow, false,
                                     player);
}

bool BoneMealItem::applyToSapling(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state, ServerPlayer *player) {
    if (state.mStates.getByte("age_bit") == 0) {
        Tag states = state.mStates;
        states.putByte("age_bit", 1);
        return BlockChangeSystem::change(level, position, BlockState(state.mName, states), BlockChangeCause::Grow,
                                         false, player);
    }

    const auto *sapling = VanillaBlocks::getAs<SaplingBlock>(state.mName);
    return sapling != nullptr && sapling->tryGrow(owner, level, position, state, player)
           && level.getBlockState(position.x, position.y, position.z).mName != state.mName;
}

bool BoneMealItem::applyToGrass(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                ServerPlayer *player) {
    if (level.getBlockState(position.x, position.y + 1, position.z).mName != BlockNames::AIR)
        return false;

    TrackingBlockManager manager(level, player);
    SimpleRandom random(randomSeed());
    LegacyTallGrass::growGrass(manager, position.x, position.y, position.z, random);

    return applyManager(owner, level, manager);
}

bool BoneMealItem::applyToNylium(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state, ServerPlayer *player) {
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

    return BlockChangeSystem::change(level, above, BlockState(grown), BlockChangeCause::Grow, false, player);
}

bool BoneMealItem::fertilize(ServerNetworkHandler &owner, Level &level, const Vector3i &position, ServerPlayer *player) {
    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    if (isCrop(state))
        return applyToCrop(owner, level, position, state, player);
    if (SaplingBlock::matches(state.mName))
        return applyToSapling(owner, level, position, state, player);
    if (state.mName == BlockNames::GRASS_BLOCK)
        return applyToGrass(owner, level, position, player);
    if (NyliumBlock::matches(state.mName))
        return applyToNylium(owner, level, position, state, player);
    const Block *block = VanillaBlocks::fromState(state);
    if (const auto *leaf = dynamic_cast<const BigDripleafBlock *>(block))
        return leaf->fertilize(owner, level, position, player);
    if (const auto *leaf = dynamic_cast<const SmallDripleafBlock *>(block))
        return leaf->fertilize(owner, level, position, state, player);
    return block != nullptr && block->onFertilized(owner, level, position, state);
}

bool BoneMealItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) face;
    (void) clickPosition;

    if (player.isSpectator())
        return false;

    Level &level = owner.getLevelFor(player);
    if (!fertilize(owner, level, blockPosition, &player))
        return false;

    player.startItemCooldown(item, owner.getCurrentTick(), USE_COOLDOWN_TICKS);

    owner.broadcastLevelEvent(level, BONE_MEAL_USE_EVENT, centerOf(blockPosition), 0);

    player.consumeOneHeldItem();

    return true;
}

bool BoneMealItem::fertilizeBlock(ServerNetworkHandler &owner, Level &level, const Vector3i &position) const {
    if (!fertilize(owner, level, position))
        return false;

    owner.broadcastLevelEvent(level, BONE_MEAL_USE_EVENT, centerOf(position), 0);
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
