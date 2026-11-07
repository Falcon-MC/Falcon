#include "World/Level/Block/Systems/PistonSystem.h"

#include "World/Level/Block/Actor/PistonArmBlockActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <algorithm>
#include <array>
#include <memory>
#include <unordered_set>

namespace {
    const char *PISTON = "minecraft:piston";
    const char *STICKY_PISTON = "minecraft:sticky_piston";
    const char *PISTON_ARM_COLLISION = "minecraft:piston_arm_collision";
    const char *STICKY_PISTON_ARM_COLLISION = "minecraft:sticky_piston_arm_collision";
    const char *AIR = "minecraft:air";

    const std::unordered_set<std::string> UNPUSHABLE = {
            "minecraft:bedrock", "minecraft:obsidian", "minecraft:crying_obsidian",
            "minecraft:glowingobsidian", "minecraft:beacon", "minecraft:barrier",
            "minecraft:allow", "minecraft:deny", "minecraft:border_block",
            "minecraft:command_block", "minecraft:chain_command_block", "minecraft:repeating_command_block",
            "minecraft:ender_chest", "minecraft:end_gateway", "minecraft:end_portal",
            "minecraft:end_portal_frame", "minecraft:frog_spawn", "minecraft:invisible_bedrock",
            "minecraft:jigsaw", "minecraft:lodestone", "minecraft:mob_spawner", "minecraft:moving_block",
            "minecraft:pointed_dripstone", "minecraft:portal", "minecraft:respawn_anchor",
            "minecraft:sculk_catalyst", "minecraft:sculk_shrieker", "minecraft:structure_block",
            "minecraft:structure_void", "minecraft:trial_spawner", "minecraft:vault",
            "minecraft:creaking_heart"
    };

    const std::unordered_set<std::string> UNPULLABLE_EXTRA = {
            "minecraft:campfire", "minecraft:soul_campfire", "minecraft:heavy_core"
    };

    const std::unordered_set<std::string> BREAKS_WHEN_MOVED = {
            "minecraft:bamboo", "minecraft:bed", "minecraft:budding_amethyst", "minecraft:cactus",
            "minecraft:cake", "minecraft:campfire", "minecraft:soul_campfire", "minecraft:candle_cake",
            "minecraft:chorus_flower", "minecraft:chorus_plant", "minecraft:cocoa", "minecraft:dragon_egg",
            "minecraft:ladder", "minecraft:lever", "minecraft:glow_lichen", "minecraft:melon_block",
            "minecraft:pumpkin", "minecraft:snow_layer", "minecraft:standing_banner",
            "minecraft:wall_banner", "minecraft:undyed_shulker_box", "minecraft:vine",
            "minecraft:frame", "minecraft:glow_frame"
    };

    const std::unordered_set<std::string> NEVER_BREAKS = {
            "minecraft:rail", "minecraft:golden_rail", "minecraft:detector_rail", "minecraft:activator_rail",
            "minecraft:sculk_sensor", "minecraft:calibrated_sculk_sensor", "minecraft:sculk_shrieker",
            "minecraft:heavy_core"
    };

    struct PendingMove {
        Vector3i mPiston;
        std::vector<Vector3i> mSources;
        std::vector<BlockState> mMoved;
        std::vector<std::unique_ptr<BlockActor>> mActors;
        int mDirection;
    };

    void closeViewersAt(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
        for (auto &entry: owner.getPlayers()) {
            if (&owner.getLevelFor(entry.second) != &level)
                continue;

            InventoryManager &manager = entry.second.getInventoryManager();
            if (manager.isContainerOpen() && manager.getContainerPosition() == position)
                manager.onClientRemoveWindow(manager.getContainerWindowId());
            if (manager.isFurnaceOpen() && manager.getFurnacePosition() == position)
                manager.onClientRemoveWindow(manager.getFurnaceWindowId());
        }
    }

    std::array<std::vector<PendingMove>, Dimension::DIMENSION_COUNT> gPendingMoves;

    bool contains(const std::unordered_set<std::string> &set, const std::string &value) {
        return set.find(value) != set.end();
    }

    PistonMoveReaction moveReactionOf(const BlockState &state) {
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        if (block == nullptr)
            return PistonMoveReaction::Normal;

        return block->getPistonMoveReaction();
    }

    bool isFlowable(const BlockState &state) {
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        if (block == nullptr)
            return false;

        const BlockData *data = block->getData();
        return data != nullptr && !data->mSolid;
    }

    BlockState stateAt(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z);
    }

    bool isAir(const BlockState &state) {
        return state.mName == AIR;
    }

    Vector3i relative(const Vector3i &position, int face, int distance = 1) {
        const Vector3i offset = RedstoneFace::offset(face);
        return Vector3i(position.x + offset.x * distance, position.y + offset.y * distance,
                        position.z + offset.z * distance);
    }

    int faceAxis(int face) {
        if (face == RedstoneFace::DOWN || face == RedstoneFace::UP)
            return 1;
        if (face == RedstoneFace::NORTH || face == RedstoneFace::SOUTH)
            return 2;
        return 0;
    }

    bool sticksTogether(const BlockState &first, const BlockState &second) {
        const bool firstSticky = PistonSystem::canSticksBlock(first);
        const bool secondSticky = PistonSystem::canSticksBlock(second);
        // slime and honey never hold onto each other
        if (firstSticky && secondSticky)
            return first.mName == second.mName;
        return firstSticky || secondSticky;
    }

    /**
     * Collects the blocks a piston moves, following slime and honey: a sticky block drags the
     * blocks behind it and beside it along, up to MOVE_BLOCK_LIMIT blocks in total.
     */
    struct MoveResolver {
        Level &mLevel;
        Vector3i mPiston;
        // the arm being retracted: it leaves before the pulled blocks arrive, so it counts as air
        Vector3i mFreed;
        bool mHasFreed;
        int mDirection;
        std::vector<Vector3i> &mToMove;
        std::vector<Vector3i> &mToDestroy;

        bool contains(const std::vector<Vector3i> &list, const Vector3i &position) const {
            return std::find(list.begin(), list.end(), position) != list.end();
        }

        bool isEmpty(const Vector3i &position, const BlockState &state) const {
            return isAir(state) || (mHasFreed && position == mFreed);
        }

        // blocks dragged along by a sticky neighbour, rather than pushed head on
        bool canBeDragged(const BlockState &state) const {
            return PistonSystem::canBePulled(state) && !PistonSystem::breaksWhenMoved(state);
        }

        bool resolve(const Vector3i &origin) {
            if (!addLine(origin, true))
                return false;

            for (size_t index = 0; index < mToMove.size(); ++index) {
                if (!addBranches(mToMove[index]))
                    return false;
            }
            return true;
        }

        bool addLine(const Vector3i &origin, bool head) {
            const BlockState originState = stateAt(mLevel, origin);
            if (isEmpty(origin, originState) || origin == mPiston || contains(mToMove, origin))
                return true;
            if (!head && !canBeDragged(originState))
                return true;

            const int back = RedstoneFace::opposite(mDirection);
            std::vector<Vector3i> line{origin};
            BlockState current = originState;
            Vector3i behind = origin;
            while (PistonSystem::canSticksBlock(current)) {
                behind = relative(behind, back);
                const BlockState behindState = stateAt(mLevel, behind);
                if (isEmpty(behind, behindState) || behind == mPiston || contains(mToMove, behind)
                    || !sticksTogether(current, behindState) || !canBeDragged(behindState))
                    break;

                line.push_back(behind);
                current = behindState;
            }

            if (mToMove.size() + line.size() > (size_t) PistonSystem::MOVE_BLOCK_LIMIT)
                return false;
            for (size_t index = line.size(); index > 0; --index)
                mToMove.push_back(line[index - 1]);

            Vector3i next = origin;
            while (true) {
                next = relative(next, mDirection);
                if (contains(mToMove, next))
                    return true;

                const BlockState nextState = stateAt(mLevel, next);
                if (isEmpty(next, nextState))
                    return true;
                if (next == mPiston || !PistonSystem::canBePushed(nextState))
                    return false;
                if (PistonSystem::breaksWhenMoved(nextState)) {
                    if (!contains(mToDestroy, next))
                        mToDestroy.push_back(next);
                    return true;
                }
                if (mToMove.size() >= (size_t) PistonSystem::MOVE_BLOCK_LIMIT)
                    return false;

                mToMove.push_back(next);
            }
        }

        bool addBranches(const Vector3i &position) {
            const BlockState state = stateAt(mLevel, position);
            if (!PistonSystem::canSticksBlock(state))
                return true;

            for (int side = 0; side < RedstoneFace::COUNT; ++side) {
                if (faceAxis(side) == faceAxis(mDirection))
                    continue;

                const Vector3i neighbour = relative(position, side);
                if (!sticksTogether(state, stateAt(mLevel, neighbour)))
                    continue;
                if (!addLine(neighbour, false))
                    return false;
            }
            return true;
        }
    };
}

bool PistonSystem::isPiston(const std::string &identifier) {
    return identifier == PISTON || identifier == STICKY_PISTON;
}

bool PistonSystem::isSticky(const std::string &identifier) {
    return identifier == STICKY_PISTON || identifier == STICKY_PISTON_ARM_COLLISION;
}

bool PistonSystem::isArmCollision(const std::string &identifier) {
    return identifier == PISTON_ARM_COLLISION || identifier == STICKY_PISTON_ARM_COLLISION;
}

/**
 * The stored facing_direction of a piston points the opposite way on the horizontal axes,
 * so it is flipped here to get the face the arm actually extends towards.
 */
int PistonSystem::getPistonFace(const BlockState &state) {
    const int index = getStoredFacing(state);
    if (index == RedstoneFace::DOWN || index == RedstoneFace::UP)
        return index;

    return RedstoneFace::opposite(index);
}

int PistonSystem::getStoredFacing(const BlockState &state) {
    const Tag *facing = state.mStates.get("facing_direction");
    if (facing == nullptr)
        return RedstoneFace::UP;

    const int index = facing->asInt();
    if (index < 0 || index >= RedstoneFace::COUNT)
        return RedstoneFace::UP;

    return index;
}

bool PistonSystem::canBePushed(const BlockState &state) {
    if (isAir(state))
        return true;
    if (contains(UNPUSHABLE, state.mName) || isArmCollision(state.mName))
        return false;

    return true;
}

bool PistonSystem::canBePulled(const BlockState &state) {
    if (!canBePushed(state))
        return false;

    return !contains(UNPULLABLE_EXTRA, state.mName) && moveReactionOf(state) != PistonMoveReaction::PushOnly;
}

bool PistonSystem::breaksWhenMoved(const BlockState &state) {
    if (isAir(state))
        return false;
    if (contains(NEVER_BREAKS, state.mName))
        return false;
    if (contains(BREAKS_WHEN_MOVED, state.mName))
        return true;
    if (moveReactionOf(state) == PistonMoveReaction::Break)
        return true;

    return isFlowable(state);
}

bool PistonSystem::sticksToPiston(const BlockState &state) {
    if (contains(NEVER_BREAKS, state.mName))
        return true;
    if (contains(BREAKS_WHEN_MOVED, state.mName))
        return false;
    if (moveReactionOf(state) != PistonMoveReaction::Normal)
        return false;

    return !isFlowable(state);
}

bool PistonSystem::canSticksBlock(const BlockState &state) {
    return state.mName == "minecraft:slime" || state.mName == "minecraft:honey_block";
}

bool PistonSystem::isExtended(Level &level, const Vector3i &position, const BlockState &state) {
    const int face = getPistonFace(state);
    const BlockState ahead = stateAt(level, relative(position, face));
    return isArmCollision(ahead.mName) && getPistonFace(ahead) == face;
}

bool PistonSystem::isGettingPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) {
    const int face = getPistonFace(state);

    for (int side = 0; side < RedstoneFace::COUNT; ++side) {
        if (side == face)
            continue;

        if (RedstoneSystem::isSidePowered(owner, level, relative(position, side), side))
            return true;
    }

    return false;
}

void PistonSystem::onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state) {
    _checkState(owner, level, position, state, isGettingPower(owner, level, position, state));
}

void PistonSystem::onBlockBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state) {
    const int face = getPistonFace(state);
    const Vector3i armPosition = relative(position, face);
    const BlockState arm = stateAt(level, armPosition);

    if (isArmCollision(arm.mName) && getPistonFace(arm) == face)
        level.setBlock(armPosition, BlockState(AIR), false);
}

bool PistonSystem::_checkState(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state, bool powered) {
    const PistonArmBlockActor *arm = level.getBlockActors().find<PistonArmBlockActor>(position);
    if (arm != nullptr && arm->isMoving())
        return false;

    const bool extended = isExtended(level, position, state);

    if (powered && !extended)
        return _doMove(owner, level, position, state, true);
    if (!powered && extended)
        return _doMove(owner, level, position, state, false);

    return false;
}

bool PistonSystem::_doMove(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state, bool extending) {
    const int face = getPistonFace(state);
    const bool sticky = isSticky(state.mName);
    const int moveDirection = extending ? face : RedstoneFace::opposite(face);
    const Vector3i armPosition = relative(position, face);

    std::vector<Vector3i> toMove;
    std::vector<Vector3i> toDestroy;

    if (extending || sticky) {
        const Vector3i origin = extending ? relative(position, face) : relative(position, face, 2);
        const BlockState originState = stateAt(level, origin);

        if (!isAir(originState)) {
            const bool pushable = extending ? canBePushed(originState) : canBePulled(originState);
            if (!pushable) {
                if (extending)
                    return false;
            } else if (breaksWhenMoved(originState)) {
                if (extending || sticksToPiston(originState))
                    toDestroy.push_back(origin);
            } else {
                MoveResolver resolver{level, position, armPosition, !extending, moveDirection, toMove, toDestroy};
                if (!resolver.resolve(origin)) {
                    if (extending)
                        return false;
                    // a retraction that cannot pull its structure still retracts, empty-handed
                    toMove.clear();
                    toDestroy.clear();
                }
            }
        }
    }

    const FalconEventType pistonEvent = extending ? FALCON_EVENT_PISTON_EXTEND : FALCON_EVENT_PISTON_RETRACT;
    if (PluginManager *plugins = PluginManager::findWithSubscribers(pistonEvent)) {
        std::vector<Vector3i> affected = toMove;
        PluginEvent event;
        event.mType = pistonEvent;
        event.mCancellable = true;
        event.mLevel = &level;
        event.mBlockPosition = position;
        event.mBlockFace = (uint32_t) face;
        event.mBlocks = &affected;
        plugins->dispatch(event);
        if (event.mCancelled)
            return false;
    }

    for (size_t index = toDestroy.size(); index > 0; --index) {
        const Vector3i &destroyed = toDestroy[index - 1];
        level.setBlock(destroyed, BlockState(AIR), false);
    }

    // Moved blocks are lifted out of the world now, with their block actors detached and any
    // open container screens closed, and only placed at their destination once the arm
    // animation completes in tick(). The client draws them in between from the arm's data.
    std::vector<BlockState> moved;
    std::vector<std::unique_ptr<BlockActor>> movedActors;
    moved.reserve(toMove.size());
    movedActors.reserve(toMove.size());
    for (const Vector3i &source: toMove) {
        moved.push_back(stateAt(level, source));
        closeViewersAt(owner, level, source);
        movedActors.push_back(level.getBlockActors().take(source));
    }

    PistonArmBlockActor &arm = level.getBlockActors().getOrCreate<PistonArmBlockActor>(position);
    arm.setSticky(sticky);
    arm.setFacing(face);
    arm.beginMove(extending, toMove);

    for (size_t index = toMove.size(); index > 0; --index)
        level.setBlock(toMove[index - 1], BlockState(AIR), false);

    gPendingMoves[level.getDimensionId()].push_back(
            PendingMove{position, toMove, std::move(moved), std::move(movedActors), moveDirection});
    BlockActionHandler::broadcastBlockActorData(owner, level, arm);

    if (extending) {
        BlockState arm(sticky ? STICKY_PISTON_ARM_COLLISION : PISTON_ARM_COLLISION);
        arm.mStates = Tag::ofCompound();
        arm.mStates.putInt("facing_direction", getStoredFacing(state));
        level.setBlock(armPosition, arm, false);
    } else if (isArmCollision(stateAt(level, armPosition).mName)) {
        level.setBlock(armPosition, BlockState(AIR), false);
    }

    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    owner.playLevelSound(level, extending ? LevelSoundEvent::PISTON_OUT : LevelSoundEvent::PISTON_IN, center);

    return true;
}

void PistonSystem::tick(ServerNetworkHandler &owner, Level &level) {
    std::vector<PendingMove> &pendingMoves = gPendingMoves[level.getDimensionId()];
    if (pendingMoves.empty())
        return;

    std::vector<PendingMove> pending;
    pending.swap(pendingMoves);

    for (PendingMove &move: pending) {
        PistonArmBlockActor *arm = level.getBlockActors().find<PistonArmBlockActor>(move.mPiston);
        if (arm != nullptr) {
            arm->advance();

            const bool done = arm->isExtending() ? arm->getProgress() >= 1.0f : arm->getProgress() <= 0.0f;
            if (!done) {
                BlockActionHandler::broadcastBlockActorData(owner, level, *arm);
                pendingMoves.push_back(std::move(move));
                continue;
            }
        }

        for (size_t index = 0; index < move.mSources.size() && index < move.mMoved.size(); ++index) {
            const Vector3i destination = relative(move.mSources[index], move.mDirection);
            level.setBlock(destination, move.mMoved[index], false);

            std::unique_ptr<BlockActor> &blockActor = move.mActors[index];
            if (blockActor == nullptr)
                continue;

            blockActor->setPosition(destination);
            const BlockActor &inserted = *blockActor;
            level.getBlockActors().insert(std::move(blockActor));
            BlockActionHandler::broadcastBlockActorData(owner, level, inserted);
        }

        for (const Vector3i &source: move.mSources) {
            RedstoneSystem::updateAroundRedstone(owner, level, source);
            RedstoneSystem::updateAroundRedstone(owner, level, relative(source, move.mDirection));
        }

        if (arm == nullptr)
            continue;

        arm->finish();
        BlockActionHandler::broadcastBlockActorData(owner, level, *arm);
        RedstoneSystem::updateAroundRedstone(owner, level, relative(move.mPiston, arm->getFacing()));
    }
}

/**
 * True while a piston move touching the area is still animating. Movement checks use it to
 * avoid judging a player against blocks that are mid-move.
 */
bool PistonSystem::isMovingNear(const Level &level, const AxisAlignedBB &area) {
    const auto touches = [&area](const Vector3i &position) {
        return position.x + 1.0f > area.mMinX && position.x < area.mMaxX && position.y + 1.0f > area.mMinY
               && position.y < area.mMaxY && position.z + 1.0f > area.mMinZ && position.z < area.mMaxZ;
    };

    for (const PendingMove &move: gPendingMoves[level.getDimensionId()]) {
        if (touches(move.mPiston) || touches(relative(move.mPiston, move.mDirection)))
            return true;
        for (const Vector3i &source: move.mSources) {
            if (touches(source) || touches(relative(source, move.mDirection)))
                return true;
        }
    }
    return false;
}
