#include "World/Level/Block/CandleBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CandleBlock, 360);

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Vibration/GameEvent.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Level.h"

#include <algorithm>

using namespace PlacementHelpers;

namespace {
    const int CANDLES_MAX = 3;
    const char *CANDLES = "candles";
    const char *const LIT = "lit";
    const char *const IGNITE_SOUND = "ignite";
    const char *const EXTINGUISH_SOUND = "extinguish.candle";
    const int32_t SMOKE_PARTICLE_EVENT = 0x4000 | 5;
    const float SMOKE_RISE = 0.125f;

    // where the flames burn from the block's corner, for one to four candles
    const Vector3f CANDLE_FLAMES[CANDLES_MAX + 1][CANDLES_MAX + 1] = {
            {Vector3f(0.5f, 0.375f, 0.5f)},
            {Vector3f(0.625f, 0.375f, 0.5f), Vector3f(0.375f, 0.3125f, 0.5625f)},
            {Vector3f(0.5625f, 0.375f, 0.4375f), Vector3f(0.375f, 0.3125f, 0.5f), Vector3f(0.5f, 0.1875f, 0.625f)},
            {Vector3f(0.5625f, 0.375f, 0.375f), Vector3f(0.375f, 0.3125f, 0.375f), Vector3f(0.625f, 0.3125f, 0.5625f),
             Vector3f(0.4375f, 0.1875f, 0.5625f)},
    };
    const Vector3f CAKE_FLAME(0.5f, 0.85f, 0.5f);

    bool isCandleCake(const std::string &identifier) {
        return identifier == "minecraft:candle_cake" || BlockIdentifier::endsWith(identifier, "_candle_cake");
    }

    void puffSmoke(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const Vector3f &flame) {
        const Vector3f at((float) position.x + flame.x, (float) position.y + flame.y + SMOKE_RISE,
                          (float) position.z + flame.z);
        owner.broadcastLevelEvent(level, SMOKE_PARTICLE_EVENT, at, 0);
    }

    Vector3f centreOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    BlockState withLit(const BlockState &state, bool lit) {
        Tag states = state.mStates;
        states.putByte(LIT, lit ? 1 : 0);
        return BlockState(state.mName, states);
    }
}

bool CandleBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:candle" || BlockIdentifier::endsWith(identifier, "_candle");
}

PlacementMergeResult CandleBlock::mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                                 const Vector3f &clickPosition, Vector3i &position,
                                                 BlockState &state) const {
    (void) blockFace;
    (void) clickPosition;

    Vector3i candidate = clickedPosition;
    BlockState existing = stateAt(level, candidate);
    if (!matches(existing.mName)) {
        const Vector3i above = PlacementOrientation::relativePosition(clickedPosition, PlacementOrientation::FACE_UP);
        const BlockState aboveState = stateAt(level, above);
        if (matches(aboveState.mName)) {
            candidate = above;
            existing = aboveState;
        } else {
            candidate = position;
            existing = stateAt(level, position);
        }
    }

    if (existing.mName == getIdentifier()) {
        const int32_t candles = existing.mStates.getInt(CANDLES, 0);
        if (candles >= CANDLES_MAX)
            return PlacementMergeResult::Rejected;

        Tag states = existing.mStates;
        states.putInt(CANDLES, candles + 1);
        position = candidate;
        state = BlockState(existing.mName, states);
        return PlacementMergeResult::Merged;
    }

    if (matches(existing.mName))
        return PlacementMergeResult::Rejected;

    return PlacementMergeResult::None;
}

bool CandleBlock::isLighter(const ItemStack &item) {
    if (item.isAir() || item.mDefinition == nullptr)
        return false;
    const std::string &identifier = item.mDefinition->getIdentifier();
    if (identifier == "minecraft:flint_and_steel")
        return true;
    // the game takes a Fire Aspect book only behind a version check
    if (identifier == "minecraft:enchanted_book")
        return false;
    return ItemEnchantments::getLevel(item, EnchantmentIds::FIRE_ASPECT) > 0;
}

bool CandleBlock::isBurning(const ServerActor &projectile) {
    return projectile.isOnFire() || projectile.getProjectileData().mFlameTicks > 0;
}

bool CandleBlock::onProjectileHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state, ServerActor &projectile) const {
    if (isBurning(projectile))
        light(owner, level, position, state, &projectile);
    return false;
}

bool CandleBlock::canBeLit(Level &level, const Vector3i &position, const BlockState &state) {
    if (state.mStates.getByte(LIT, 0) != 0)
        return false;
    return !LiquidView(level.getBlockStateAtLayer(position.x, position.y, position.z, 1)).isWater();
}

bool CandleBlock::light(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                        Actor *source) {
    if (!canBeLit(level, position, state))
        return false;
    level.setBlock(position, withLit(state, true), true);
    owner.playLevelSound(level, IGNITE_SOUND, centreOf(position));
    level.postGameEvent(GameEvents::BLOCK_CHANGE, centreOf(position), source);
    return true;
}

void CandleBlock::extinguish(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state, Actor *source) {
    level.setBlock(position, withLit(state, false), true);
    if (isCandleCake(state.mName)) {
        puffSmoke(owner, level, position, CAKE_FLAME);
    } else {
        const int32_t candles = std::clamp(state.mStates.getInt(CANDLES, 0), 0, CANDLES_MAX);
        for (int32_t flame = 0; flame <= candles; ++flame)
            puffSmoke(owner, level, position, CANDLE_FLAMES[candles][flame]);
    }
    owner.playLevelSound(level, EXTINGUISH_SOUND, centreOf(position));
    level.postGameEvent(GameEvents::BLOCK_CHANGE, centreOf(position), source);
}

// anything but a lighter or an empty hand goes on to the item: another candle is placed
bool CandleBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                             const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    const ItemStack held = player.getInventory().getItemInHand();
    if (isLighter(held)) {
        if (light(owner, level, position, state, &player))
            owner.damagePlayerHeldItem(player, 1);
        return true;
    }
    if (!held.isAir() || state.mStates.getByte(LIT, 0) == 0)
        return false;
    extinguish(owner, level, position, state, &player);
    return true;
}
