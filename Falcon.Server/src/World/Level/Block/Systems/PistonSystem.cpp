#include "World/Level/Block/Systems/PistonSystem.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorCollisionSystem.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Level/Block/Actor/PistonArmBlockActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/PistonMovable.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "World/Level/Block/BlockNames.h"

#include <map>
#include <unordered_map>
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

namespace {
    const char *PISTON = "minecraft:piston";
    const char *STICKY_PISTON = "minecraft:sticky_piston";
    const char *PISTON_ARM_COLLISION = "minecraft:piston_arm_collision";
    const char *STICKY_PISTON_ARM_COLLISION = "minecraft:sticky_piston_arm_collision";
    using BlockNames::AIR;

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

    // per level: worlds share dimension ids
    std::unordered_map<const Level *, std::vector<PendingMove>> gPendingMoves;

    // the pistons whose power asks for a move they could not make yet: blocked, cancelled or still moving. The
    // circuit sends a steady strength only once, so each tick looks at them again
    std::unordered_map<const Level *, std::map<int64_t, Vector3i>> gWaiting;

    int64_t waitKey(const Vector3i &position) {
        return ((int64_t) position.x << 38) ^ ((int64_t) (position.z & 0x3FFFFFF) << 12) ^ (int64_t) (position.y & 0xFFF);
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

    const char *const PUSHABLE_BY_BLOCK = "minecraft:pushable_by_block";

    // mobs with minecraft:pushable_by_block in the cells the moving blocks and the arm head enter go one block
    // along; the game slides them over the two ticks of the move
    void pushMobs(ServerNetworkHandler &owner, Level &level, const Vector3i &piston, const std::vector<Vector3i> &moved,
                  const std::optional<Vector3i> &armHead, int direction) {
        std::vector<AxisAlignedBB> cells;
        cells.reserve(moved.size() + 1);
        for (const Vector3i &source: moved) {
            const Vector3i target = relative(source, direction);
            cells.emplace_back((float) target.x, (float) target.y, (float) target.z, (float) target.x + 1.0f,
                               (float) target.y + 1.0f, (float) target.z + 1.0f);
        }
        if (armHead.has_value())
            cells.emplace_back((float) armHead->x, (float) armHead->y, (float) armHead->z, (float) armHead->x + 1.0f,
                               (float) armHead->y + 1.0f, (float) armHead->z + 1.0f);
        if (cells.empty())
            return;

        const Vector3i offset = RedstoneFace::offset(direction);
        const Vector3f center((float) piston.x + 0.5f, (float) piston.y + 0.5f, (float) piston.z + 0.5f);
        std::vector<MobActor *> pushed;
        owner.forEachActorNear(center, (float) PistonSystem::MOVE_BLOCK_LIMIT + 3.0f, [&](ServerActor &actor) {
            MobActor *mob = dynamic_cast<MobActor *>(&actor);
            if (mob == nullptr || !mob->isAlive() || &owner.getLevelFor(*mob) != &level
                || mob->getComponent(PUSHABLE_BY_BLOCK) == nullptr)
                return;
            const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(*mob);
            for (const AxisAlignedBB &cell: cells) {
                if (box.intersectsWith(cell)) {
                    pushed.push_back(mob);
                    return;
                }
            }
        });
        const Vector3f delta((float) offset.x, (float) offset.y, (float) offset.z);
        for (MobActor *mob: pushed) {
            // moved blocks are air until the move lands: only the blocks past them stop the mob
            AxisAlignedBB box = ActorPushSystem::boundingBoxOf(*mob);
            const ActorMoveResult result = ActorCollisionSystem::move(level, box, delta, 0.0f, mob->isOnGround(), mob);
            const Vector3f &position = mob->getPosition();
            mob->teleport(Vector3f(position.x + result.mMoved.x, position.y + result.mMoved.y,
                                   position.z + result.mMoved.z));
        }
    }

    int faceAxis(int face) {
        if (face == RedstoneFace::DOWN || face == RedstoneFace::UP)
            return 1;
        if (face == RedstoneFace::NORTH || face == RedstoneFace::SOUTH)
            return 2;
        return 0;
    }

    // slime and honey reach every neighbour
    // but the one walked from; along the move direction pushed, from a sticky side only dragged if pullable
    struct MoveResolver {
        Level &mLevel;
        Vector3i mPiston;
        // the retracting arm: removed by the game before walking the pulled blocks, counts as air
        Vector3i mFreed;
        bool mHasFreed;
        int mDirection;
        std::vector<Vector3i> &mToMove;
        std::vector<Vector3i> &mToDestroy;

        bool contains(const std::vector<Vector3i> &list, const Vector3i &position) const {
            return std::find(list.begin(), list.end(), position) != list.end();
        }

        bool isInWorld(const Vector3i &position) const {
            return position.y >= mLevel.getMinY() && position.y <= mLevel.getMaxY();
        }

        // a piston's block actor lets it be moved only while it is retracted and still
        bool isHeldPiston(const Vector3i &position, const BlockState &state) const {
            if (!PistonSystem::isPiston(state.mName))
                return false;
            const PistonArmBlockActor *arm = mLevel.getBlockActors().find<PistonArmBlockActor>(position);
            return (arm != nullptr && arm->isMoving()) || PistonSystem::isExtended(mLevel, position, state);
        }

        // the walk starts with the piston's facing as the branch: ahead when pushing, behind when pulling
        bool resolve(const Vector3i &origin, int facing) {
            return walk(origin, facing) && mToMove.size() <= (size_t) PistonSystem::MOVE_BLOCK_LIMIT;
        }

        bool walk(const Vector3i &position, int branch) {
            if (contains(mToMove, position))
                return true;

            const BlockState state = stateAt(mLevel, position);
            if (isAir(state) || (mHasFreed && position == mFreed))
                return true;

            const bool ahead = branch == mDirection;
            // the moving piston, an immovable block and a piston that is out or moving hold: they stop a push and
            // stay behind on a pull
            if (position == mPiston || !PistonSystem::canBePushed(state) || isHeldPiston(position, state))
                return !ahead;

            if (PistonSystem::canSticksBlock(state)) {
                if (!isInWorld(relative(position, mDirection)))
                    return false;
                mToMove.push_back(position);
                // past the limit the structure fails anyway
                if (mToMove.size() > (size_t) PistonSystem::MOVE_BLOCK_LIMIT)
                    return false;

                const int from = RedstoneFace::opposite(branch);
                for (int side = 0; side < RedstoneFace::COUNT; ++side) {
                    if (side == from)
                        continue;

                    const Vector3i neighbour = relative(position, side);
                    const BlockState neighbourState = stateAt(mLevel, neighbour);
                    // slime and honey do not hold onto each other, but one still pushes the other ahead of it
                    if (side != mDirection && PistonSystem::canSticksBlock(neighbourState)
                        && neighbourState.mName != state.mName)
                        continue;
                    if (!walk(neighbour, side))
                        return false;
                }
                return true;
            }

            if (PistonSystem::breaksWhenMoved(state)) {
                if (ahead && !contains(mToDestroy, position))
                    mToDestroy.push_back(position);
                return true;
            }
            if (!ahead && !PistonSystem::canBePulled(state))
                return true;

            mToMove.push_back(position);
            if (mToMove.size() > (size_t) PistonSystem::MOVE_BLOCK_LIMIT)
                return false;

            const Vector3i next = relative(position, mDirection);
            return isInWorld(next) && walk(next, mDirection);
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

// stored facing_direction is reversed on the horizontal axes
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
    return isAir(state) || PistonMovable::of(state.mName).mMovement != PistonMovement::Immovable;
}

bool PistonSystem::canBePulled(const BlockState &state) {
    return !isAir(state) && PistonMovable::of(state.mName).mMovement == PistonMovement::PushPull;
}

bool PistonSystem::breaksWhenMoved(const BlockState &state) {
    return !isAir(state) && PistonMovable::of(state.mName).mMovement == PistonMovement::Popped;
}

bool PistonSystem::canSticksBlock(const BlockState &state) {
    return PistonMovable::of(state.mName).mSticky;
}

bool PistonSystem::isExtended(Level &level, const Vector3i &position, const BlockState &state) {
    const int face = getPistonFace(state);
    const BlockState ahead = stateAt(level, relative(position, face));
    return isArmCollision(ahead.mName) && getPistonFace(ahead) == face;
}

// the piston's circuit component takes no power through its face
bool PistonSystem::isGettingPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) {
    (void) state;
    return RedstoneSystem::isGettingPower(owner, level, position);
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
    const bool extended = isExtended(level, position, state);
    const PistonArmBlockActor *arm = level.getBlockActors().find<PistonArmBlockActor>(position);
    const bool moving = arm != nullptr && arm->isMoving();
    if (powered == extended && !moving) {
        const auto found = gWaiting.find(&level);
        if (found != gWaiting.end())
            found->second.erase(waitKey(position));
        return false;
    }
    if (!moving && _doMove(owner, level, position, state, powered)) {
        const auto found = gWaiting.find(&level);
        if (found != gWaiting.end())
            found->second.erase(waitKey(position));
        return true;
    }
    gWaiting[&level][waitKey(position)] = position;
    return false;
}

bool PistonSystem::_doMove(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                           bool extending) {
    const int face = getPistonFace(state);
    const bool sticky = isSticky(state.mName);
    const int moveDirection = extending ? face : RedstoneFace::opposite(face);
    const Vector3i armPosition = relative(position, face);

    std::vector<Vector3i> toMove;
    std::vector<Vector3i> toDestroy;

    if (extending || sticky) {
        const Vector3i origin = extending ? relative(position, face) : relative(position, face, 2);
        MoveResolver resolver{level, position, armPosition, !extending, moveDirection, toMove, toDestroy};
        if (!resolver.resolve(origin, face)) {
            if (extending)
                return false;
            // the game still retracts a sticky piston whose blocks cannot be pulled: it pulls nothing
            toMove.clear();
            toDestroy.clear();
        }
    }

    BeforeEvents &before = owner.getEventBus().before();
    EventSignal<PistonMoveBeforeEvent> &pistonMove = extending ? before.mPistonExtend : before.mPistonRetract;
    if (pistonMove.hasSubscribers()) {
        PistonMoveBeforeEvent event(level, position, face, toMove);
        pistonMove.emit(event);
        if (event.isCancelled())
            return false;
    }

    for (size_t index = toDestroy.size(); index > 0; --index) {
        const Vector3i &destroyed = toDestroy[index - 1];
        const BlockState broken = level.getBlockState(destroyed.x, destroyed.y, destroyed.z);
        BlockActionHandler::destroyBlock(owner, level, destroyed, broken, true, ItemStack::air(), nullptr, &position);
    }

    // lifted out now (actors detached, container screens closed), placed when the arm animation ends in
    // tick(); the client draws them meanwhile from the arm's data
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
    owner.getScriptEngine().onPistonActivate(level, position, extending);

    for (size_t index = toMove.size(); index > 0; --index)
        level.setBlock(toMove[index - 1], BlockState(AIR), false);

    pushMobs(owner, level, position, toMove, extending ? std::optional<Vector3i>(armPosition) : std::nullopt,
             moveDirection);
    gPendingMoves[&level].push_back(
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
    level.postGameEvent(extending ? GameEvents::PISTON_EXTEND : GameEvents::PISTON_CONTRACT, center);

    (void) faceAxis;
    return true;
}

namespace {
    void completeMove(ServerNetworkHandler &owner, Level &level, PendingMove &move) {
        for (size_t index = 0; index < move.mSources.size() && index < move.mMoved.size(); ++index) {
            const Vector3i destination = relative(move.mSources[index], move.mDirection);
            level.setBlock(destination, move.mMoved[index], false);
            RedstoneSystem::onObserverMoved(owner, level, destination);

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

        PistonArmBlockActor *arm = level.getBlockActors().find<PistonArmBlockActor>(move.mPiston);
        if (arm == nullptr)
            return;

        arm->finish();
        BlockActionHandler::broadcastBlockActorData(owner, level, *arm);
        RedstoneSystem::updateAroundRedstone(owner, level, relative(move.mPiston, arm->getFacing()));
    }
}

void PistonSystem::tick(ServerNetworkHandler &owner, Level &level) {
    const auto waiting = gWaiting.find(&level);
    if (waiting != gWaiting.end() && !waiting->second.empty()) {
        std::vector<Vector3i> positions;
        for (const auto &entry: waiting->second)
            positions.push_back(entry.second);
        waiting->second.clear();
        for (const Vector3i &position: positions) {
            if (!level.isChunkResident(position.x >> 4, position.z >> 4))
                continue;
            const BlockState state = stateAt(level, position);
            if (isPiston(state.mName))
                _checkState(owner, level, position, state, isGettingPower(owner, level, position, state));
        }
    }

    const auto found = gPendingMoves.find(&level);
    if (found == gPendingMoves.end() || found->second.empty())
        return;

    std::vector<PendingMove> pending;
    pending.swap(found->second);

    for (PendingMove &move: pending) {
        PistonArmBlockActor *arm = level.getBlockActors().find<PistonArmBlockActor>(move.mPiston);
        if (arm != nullptr) {
            arm->advance();

            const bool done = arm->isExtending() ? arm->getProgress() >= 1.0f : arm->getProgress() <= 0.0f;
            if (!done) {
                BlockActionHandler::broadcastBlockActorData(owner, level, *arm);
                gPendingMoves[&level].push_back(std::move(move));
                continue;
            }
        }

        completeMove(owner, level, move);
    }
}

// the moved blocks live only in memory until the arm stops: placed now, before anything is saved or unloaded
void PistonSystem::finishMoves(ServerNetworkHandler &owner, Level &level, const std::unordered_set<int64_t> *columns) {
    const auto found = gPendingMoves.find(&level);
    if (found == gPendingMoves.end())
        return;

    const auto inColumns = [columns](const Vector3i &position) {
        return columns->count(((int64_t) (position.x >> 4) << 32) | (uint32_t) (position.z >> 4)) != 0;
    };

    std::vector<PendingMove> pending;
    pending.swap(found->second);
    gPendingMoves.erase(found);

    for (PendingMove &move: pending) {
        bool touched = columns == nullptr || inColumns(move.mPiston);
        for (size_t index = 0; index < move.mSources.size() && !touched; ++index)
            touched = inColumns(move.mSources[index]) || inColumns(relative(move.mSources[index], move.mDirection));
        if (!touched) {
            gPendingMoves[&level].push_back(std::move(move));
            continue;
        }

        completeMove(owner, level, move);
    }
}

void PistonSystem::forgetLevel(const Level &level) {
    gPendingMoves.erase(&level);
    gWaiting.erase(&level);
}

// the game fills each destination with a moving_block, which nothing can be placed into
bool PistonSystem::isMovingInto(const Level &level, const Vector3i &position) {
    const auto found = gPendingMoves.find(&level);
    if (found == gPendingMoves.end())
        return false;

    for (const PendingMove &move: found->second) {
        for (const Vector3i &source: move.mSources) {
            if (relative(source, move.mDirection) == position)
                return true;
        }
    }
    return false;
}

// movement checks: never judge a player against blocks mid-move
bool PistonSystem::isMovingNear(const Level &level, const AxisAlignedBB &area) {
    const auto touches = [&area](const Vector3i &position) {
        return position.x + 1.0f > area.mMinX && position.x < area.mMaxX && position.y + 1.0f > area.mMinY
               && position.y < area.mMaxY && position.z + 1.0f > area.mMinZ && position.z < area.mMaxZ;
    };

    const auto found = gPendingMoves.find(&level);
    if (found == gPendingMoves.end())
        return false;

    for (const PendingMove &move: found->second) {
        if (touches(move.mPiston) || touches(relative(move.mPiston, move.mDirection)))
            return true;
        for (const Vector3i &source: move.mSources) {
            if (touches(source) || touches(relative(source, move.mDirection)))
                return true;
        }
    }
    return false;
}
