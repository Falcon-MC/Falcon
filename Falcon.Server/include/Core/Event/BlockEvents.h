#pragma once

#include "Core/Event/Event.h"
#include "Core/Event/EventSignal.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"

#include <cstdint>
#include <vector>

class Actor;
class Level;
class ServerPlayer;

// a block about to grow, spread, form, fade or decay into another; the signal tells which
struct BlockChangeBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    const BlockState &mState;
    // null when the chunk is not loaded
    const BlockState *mCurrent;
    // the block it spreads from, null for the other changes
    const Vector3i *mSource;
    ServerPlayer *mPlayer;

    BlockChangeBeforeEvent(Level &level, const Vector3i &position, const BlockState &state, const BlockState *current,
                           const Vector3i *source, ServerPlayer *player)
            : mLevel(level), mBlockPosition(position), mState(state), mCurrent(current), mSource(source),
              mPlayer(player) {}
};

// a tree, mushroom or plant about to place all its blocks; a handler may replace the list
struct StructureGrowBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mOrigin;
    BlockChangeSystem::StructureBlocks &mBlocks;
    ServerPlayer *mPlayer;

    StructureGrowBeforeEvent(Level &level, const Vector3i &origin, BlockChangeSystem::StructureBlocks &blocks,
                             ServerPlayer *player)
            : mLevel(level), mOrigin(origin), mBlocks(blocks), mPlayer(player) {}
};

// a liquid about to flow into a block
struct LiquidFlowBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mSource;
    Vector3i mBlockPosition;
    const BlockState &mLiquid;

    LiquidFlowBeforeEvent(Level &level, const Vector3i &source, const Vector3i &position, const BlockState &liquid)
            : mLevel(level), mSource(source), mBlockPosition(position), mLiquid(liquid) {}
};

// a redstone wire or pressure plate about to change its power; a handler may change the new power
struct RedstoneChangeBeforeEvent : public Event {
    Level &mLevel;
    Vector3i mBlockPosition;
    int32_t mPreviousPower;
    int32_t mPower;

    RedstoneChangeBeforeEvent(Level &level, const Vector3i &position, int32_t previousPower, int32_t power)
            : mLevel(level), mBlockPosition(position), mPreviousPower(previousPower), mPower(power) {}
};

// a pressure plate about to take its new state; a handler may replace the state
struct PressurePlateUpdateBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    int32_t mPreviousStrength;
    int32_t mStrength;
    BlockState &mUpdated;
    // runtime ids of the actors on the plate
    const std::vector<uint64_t> &mActivators;
    // false when a cancelled update must not check the plate again
    bool mRecheck = true;

    PressurePlateUpdateBeforeEvent(Level &level, const Vector3i &position, int32_t previousStrength, int32_t strength,
                                   BlockState &updated, const std::vector<uint64_t> &activators)
            : mLevel(level), mBlockPosition(position), mPreviousStrength(previousStrength), mStrength(strength),
              mUpdated(updated), mActivators(activators) {}
};

// a block about to explode, before it is removed
struct BlockExplodeBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    ServerPlayer *mPlayer;
    double mRadius;
    bool mBreaksBlocks;
    double mFireChance;

    BlockExplodeBeforeEvent(Level &level, const Vector3i &position, ServerPlayer *player, double radius,
                            bool breaksBlocks, double fireChance)
            : mLevel(level), mBlockPosition(position), mPlayer(player), mRadius(radius), mBreaksBlocks(breaksBlocks),
              mFireChance(fireChance) {}
};

// a player using a charged respawn anchor; it sets the spawn or explodes
struct RespawnAnchorUseBeforeEvent : public CancellableEvent {
    Level &mLevel;
    ServerPlayer &mPlayer;
    Vector3i mBlockPosition;
    bool mSetsSpawn;

    RespawnAnchorUseBeforeEvent(Level &level, ServerPlayer &player, const Vector3i &position, bool setsSpawn)
            : mLevel(level), mPlayer(player), mBlockPosition(position), mSetsSpawn(setsSpawn) {}
};

// farmland about to change its moisture
struct FarmlandHydrationChangeBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    int32_t mPreviousMoisture;
    int32_t mMoisture;

    FarmlandHydrationChangeBeforeEvent(Level &level, const Vector3i &position, int32_t previousMoisture,
                                       int32_t moisture)
            : mLevel(level), mBlockPosition(position), mPreviousMoisture(previousMoisture), mMoisture(moisture) {}
};

// an actor falling on farmland, about to turn it to dirt
struct FarmlandTrampleBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Actor &mEntity;
    Vector3i mBlockPosition;

    FarmlandTrampleBeforeEvent(Level &level, Actor &entity, const Vector3i &position)
            : mLevel(level), mEntity(entity), mBlockPosition(position) {}
};

// fire about to burn a block or spread to one; the signal tells which
struct FireBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    const BlockState &mBlock;
    Vector3i mFirePosition;

    FireBeforeEvent(Level &level, const Vector3i &position, const BlockState &block, const Vector3i &firePosition)
            : mLevel(level), mBlockPosition(position), mBlock(block), mFirePosition(firePosition) {}
};

// a piston about to extend or retract; the signal tells which
struct PistonMoveBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    int32_t mFace;
    const std::vector<Vector3i> &mMoved;

    PistonMoveBeforeEvent(Level &level, const Vector3i &position, int32_t face, const std::vector<Vector3i> &moved)
            : mLevel(level), mBlockPosition(position), mFace(face), mMoved(moved) {}
};

// a block told a neighbour changed, before it reacts
struct BlockUpdateBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;

    BlockUpdateBeforeEvent(Level &level, const Vector3i &position) : mLevel(level), mBlockPosition(position) {}
};

// a nether portal about to fill its frame
struct PortalCreateBeforeEvent : public CancellableEvent {
    Level &mLevel;
    const std::vector<Vector3i> &mBlocks;

    PortalCreateBeforeEvent(Level &level, const std::vector<Vector3i> &blocks) : mLevel(level), mBlocks(blocks) {}
};

// a dragon egg about to teleport; a handler may move the destination
struct BlockTeleportBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    Vector3i mDestination;

    BlockTeleportBeforeEvent(Level &level, const Vector3i &position, const Vector3i &destination)
            : mLevel(level), mBlockPosition(position), mDestination(destination) {}
};

// a dispenser or dropper about to fire the item of one slot
struct BlockDispenseBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    const BlockState &mBlock;
    const ItemStack &mItem;

    BlockDispenseBeforeEvent(Level &level, const Vector3i &position, const BlockState &block, const ItemStack &item)
            : mLevel(level), mBlockPosition(position), mBlock(block), mItem(item) {}
};

// a coral out of water about to die; a handler may replace the dead block
struct BlockDeathBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    const BlockState &mCurrent;
    BlockState &mNext;

    BlockDeathBeforeEvent(Level &level, const Vector3i &position, const BlockState &current, BlockState &next)
            : mLevel(level), mBlockPosition(position), mCurrent(current), mNext(next) {}
};

// two chests about to join into a double chest
struct ChestPairBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mLeft;
    Vector3i mRight;

    ChestPairBeforeEvent(Level &level, const Vector3i &left, const Vector3i &right)
            : mLevel(level), mLeft(left), mRight(right) {}
};

// a player about to lie down in a bed
struct PlayerBedEnterBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    // the head half
    Vector3i mBedPosition;

    PlayerBedEnterBeforeEvent(ServerPlayer &player, const Vector3i &bedPosition)
            : mPlayer(player), mBedPosition(bedPosition) {}
};

// a hopper about to move one item of a stack, pushing it out or pulling it in
struct HopperTransferBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    // the whole stack the item comes from
    const ItemStack &mSource;
    bool mPushing;

    HopperTransferBeforeEvent(Level &level, const Vector3i &position, const ItemStack &source, bool pushing)
            : mLevel(level), mBlockPosition(position), mSource(source), mPushing(pushing) {}
};

// a campfire slot done cooking, before the item turns into its result
struct CampfireCookBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    int32_t mSlot;
    const ItemStack &mInput;
    ItemStack &mResult;

    CampfireCookBeforeEvent(Level &level, const Vector3i &position, int32_t slot, const ItemStack &input,
                            ItemStack &result)
            : mLevel(level), mBlockPosition(position), mSlot(slot), mInput(input), mResult(result) {}
};

struct BlockBeforeSignals {
    EventSignal<BlockChangeBeforeEvent> mBlockGrow;
    EventSignal<BlockChangeBeforeEvent> mBlockSpread;
    EventSignal<BlockChangeBeforeEvent> mBlockForm;
    EventSignal<BlockChangeBeforeEvent> mBlockFade;
    EventSignal<BlockChangeBeforeEvent> mLeavesDecay;
    EventSignal<StructureGrowBeforeEvent> mStructureGrow;
    EventSignal<LiquidFlowBeforeEvent> mLiquidFlow;
    EventSignal<RedstoneChangeBeforeEvent> mRedstoneChange;
    EventSignal<PressurePlateUpdateBeforeEvent> mPressurePlateUpdate;
    EventSignal<BlockExplodeBeforeEvent> mBlockExplode;
    EventSignal<RespawnAnchorUseBeforeEvent> mRespawnAnchorUse;
    EventSignal<FarmlandHydrationChangeBeforeEvent> mFarmlandHydrationChange;
    EventSignal<FarmlandTrampleBeforeEvent> mFarmlandTrample;
    EventSignal<FireBeforeEvent> mBlockBurn;
    EventSignal<FireBeforeEvent> mFireSpread;
    EventSignal<PistonMoveBeforeEvent> mPistonExtend;
    EventSignal<PistonMoveBeforeEvent> mPistonRetract;
    EventSignal<BlockUpdateBeforeEvent> mBlockUpdate;
    EventSignal<PortalCreateBeforeEvent> mPortalCreate;
    EventSignal<BlockTeleportBeforeEvent> mBlockTeleport;
    EventSignal<BlockDispenseBeforeEvent> mBlockDispense;
    EventSignal<BlockDeathBeforeEvent> mBlockDeath;
    EventSignal<ChestPairBeforeEvent> mChestPair;
    EventSignal<PlayerBedEnterBeforeEvent> mPlayerBedEnter;
    EventSignal<HopperTransferBeforeEvent> mHopperTransfer;
    EventSignal<CampfireCookBeforeEvent> mCampfireCook;

    void clearBlockBeforeSignals() {
        mBlockGrow.clear();
        mBlockSpread.clear();
        mBlockForm.clear();
        mBlockFade.clear();
        mLeavesDecay.clear();
        mStructureGrow.clear();
        mLiquidFlow.clear();
        mRedstoneChange.clear();
        mPressurePlateUpdate.clear();
        mBlockExplode.clear();
        mRespawnAnchorUse.clear();
        mFarmlandHydrationChange.clear();
        mFarmlandTrample.clear();
        mBlockBurn.clear();
        mFireSpread.clear();
        mPistonExtend.clear();
        mPistonRetract.clear();
        mBlockUpdate.clear();
        mPortalCreate.clear();
        mBlockTeleport.clear();
        mBlockDispense.clear();
        mBlockDeath.clear();
        mChestPair.clear();
        mPlayerBedEnter.clear();
        mHopperTransfer.clear();
        mCampfireCook.clear();
    }
};

struct BlockAfterSignals {

    void clearBlockAfterSignals() {}
};
