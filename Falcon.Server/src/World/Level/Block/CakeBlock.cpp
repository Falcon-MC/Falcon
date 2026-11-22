#include "World/Level/Block/CakeBlock.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/CandleBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <algorithm>

FALCON_REGISTER_BLOCK(CakeBlock, 100);
FALCON_REGISTER_BLOCK(CandleCakeBlock, 361);

namespace {
    const char *const BITE_COUNTER = "bite_counter";
    const char *const LIT = "lit";
    const char *const CANDLE_CAKE_SUFFIX = "_cake";
    const char *const ADD_CANDLE_SOUND = "cake.add_candle";
    const char *const BURP_SOUND = "burp";

    Vector3f centreOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool hasFloor(Level &level, const Vector3i &position) {
        const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
        return (BlockMaterial::flagsOf(below) & BlockMaterial::FLAG_SUPPORTS_CAKE) != 0;
    }
}

bool CakeBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::CAKE;
}

// two less per slice eaten
int CakeBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    return 14 - 2 * std::clamp(state.mStates.getInt(BITE_COUNTER, 0), 0, MAX_BITES);
}

std::string CakeBlock::candleCakeFor(const std::string &candleIdentifier) {
    if (!CandleBlock::matches(candleIdentifier))
        return std::string();
    return candleIdentifier + CANDLE_CAKE_SUFFIX;
}

bool CakeBlock::canEat(ServerNetworkHandler &owner, const ServerPlayer &player) {
    return player.isCreative() || owner.getProperties().getDifficulty() == Difficulty::Peaceful || player.isHungry();
}

void CakeBlock::eatSlice(ServerNetworkHandler &owner, ServerPlayer &player, Level &level, const Vector3i &position,
                         const BlockState &cake) {
    player.consumeFood(SLICE_NUTRITION, SLICE_SATURATION);
    const int32_t bites = std::clamp(cake.mStates.getInt(BITE_COUNTER, 0), 0, MAX_BITES);
    if (bites < MAX_BITES) {
        Tag states = cake.mStates;
        states.putInt(BITE_COUNTER, bites + 1);
        level.setBlock(position, BlockState(cake.mName, states), true);
        level.postGameEvent(GameEvents::BLOCK_CHANGE, centreOf(position), &player);
    } else {
        level.setBlock(position, BlockState(BlockNames::AIR), true);
        level.postGameEvent(GameEvents::BLOCK_DESTROY, centreOf(position), &player);
    }
    owner.syncPlayerAttributes(player);
    owner.playLevelSound(level, BURP_SOUND, player.getPosition());
}

bool CakeBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                           const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    const ItemStack held = player.getInventory().getItemInHand();
    const std::string candleCake = held.isAir() || held.mDefinition == nullptr
                                           ? std::string()
                                           : candleCakeFor(held.mDefinition->getIdentifier());
    const Block *topped = candleCake.empty() ? nullptr : VanillaBlocks::fromIdentifier(candleCake);
    if (topped != nullptr && state.mStates.getInt(BITE_COUNTER, 0) == 0) {
        owner.playLevelSound(level, ADD_CANDLE_SOUND, centreOf(position));
        level.setBlock(position, topped->toBlockState(), true);
        level.postGameEvent(GameEvents::BLOCK_CHANGE, centreOf(position), &player);
        player.consumeOneHeldItem();
        return true;
    }

    if (canEat(owner, player))
        eatSlice(owner, player, level, position, state);
    return true;
}

bool CakeBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    return hasFloor(level, position);
}

void CakeBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state) const {
    if (!canSurvive(level, position, state))
        BlockActionHandler::destroyBlock(owner, level, position, state, false, ItemStack::air());
}

bool CandleCakeBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:candle_cake" || BlockIdentifier::endsWith(identifier, "_candle_cake");
}

int CandleCakeBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    (void) state;
    return 14;
}

bool CandleCakeBlock::onProjectileHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state, ServerActor &projectile) const {
    if (CandleBlock::isBurning(projectile))
        CandleBlock::light(owner, level, position, state, &projectile);
    return false;
}

bool CandleCakeBlock::onWindBurst(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    return CandleBlock::putOutByWind(owner, level, position, state);
}

std::string CandleCakeBlock::candleOf(const std::string &candleCakeIdentifier) {
    const std::string suffix = CANDLE_CAKE_SUFFIX;
    if (candleCakeIdentifier.size() <= suffix.size())
        return std::string();
    return candleCakeIdentifier.substr(0, candleCakeIdentifier.size() - suffix.size());
}

bool CandleCakeBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                 const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    const ItemStack held = player.getInventory().getItemInHand();
    const bool lit = state.mStates.getByte(LIT, 0) != 0;
    if (CandleBlock::isLighter(held)) {
        if (!lit && CandleBlock::light(owner, level, position, state, &player))
            owner.damagePlayerHeldItem(player, 1);
        return true;
    }
    // the game lets a fire charge light it through the item's own use
    if (!held.isAir() && held.mDefinition != nullptr && held.mDefinition->getIdentifier() == "minecraft:fire_charge")
        return false;
    if (held.isAir() && lit) {
        CandleBlock::extinguish(owner, level, position, state, &player);
        return true;
    }
    if (!CakeBlock::canEat(owner, player))
        return false;

    const ItemStack candle = owner.createItemStack(candleOf(state.mName), 1);
    if (!candle.isAir())
        ItemActorHandler::dropItem(owner, level, ItemActorHandler::randomPopPosition(position), candle,
                                   ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
    const Block *cake = VanillaBlocks::fromIdentifier(BlockNames::CAKE);
    if (cake == nullptr)
        return true;
    const BlockState whole = cake->toBlockState();
    level.setBlock(position, whole, false);
    CakeBlock::eatSlice(owner, player, level, position, whole);
    return true;
}

bool CandleCakeBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    return hasFloor(level, position);
}

void CandleCakeBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    if (!canSurvive(level, position, state))
        BlockActionHandler::destroyBlock(owner, level, position, state, false, ItemStack::air());
}

bool CandleCakeBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                               std::vector<BlockDrop> &drops) const {
    (void) tool;
    (void) fortuneLevel;
    drops.push_back(BlockDrop{candleOf(state.mName), 1});
    return true;
}
