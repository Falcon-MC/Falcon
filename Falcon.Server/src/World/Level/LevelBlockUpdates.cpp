#include "World/Level/BlockUpdates.h"
#include "World/Level/Level.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/Systems/DetectorSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/CommandBlockSystem.h"
#include "World/Level/Block/Systems/FallingBlockSystem.h"
#include "World/Level/Block/Systems/FireSystem.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/CommandBlock.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <unordered_map>

#include <typeinfo>

namespace {
    bool isChunkReady(Level &level, const Vector3i &position) {
        if (position.y < level.getMinY() || position.y > level.getMaxY())
            return false;

        return level.isChunkResident(position.x >> 4, position.z >> 4);
    }
}

void Level::setBlock(const Vector3i &position, const BlockState &state, bool update) {
    if (!isChunkReady(*this, position))
        return;

    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    const BlockState placed = block != nullptr && block->hasDynamicShape() ? block->updateShape(*this, position, state)
                                                                           : state;
    const CustomContentRegistry &custom = CustomContentRegistry::getInstance();
    const bool placesCustom = block == nullptr && !custom.getBlocks().empty() && custom.isCustomBlock(placed.mName);
    const BlockState previous = placesCustom ? getBlockState(position.x, position.y, position.z) : BlockState();
    setBlockState(position.x, position.y, position.z, placed);

    if (mOwner != nullptr)
        BlockActionHandler::broadcastBlockUpdate(*mOwner, *this, position, placed);

    if (placesCustom && mOwner != nullptr && previous.mName != placed.mName)
        mOwner->getScriptEngine().onCustomBlockPlaced(*this, position, placed, previous);
    else if (placesCustom && mOwner != nullptr && !(previous.mStates == placed.mStates))
        mOwner->getScriptEngine().onCustomBlockStateChange(*this, position, placed, previous);

    if (!update)
        return;

    updateAt(position, BlockUpdateType::Normal);
    updateAround(position);
}

void Level::updateAround(const Vector3i &position) {
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        updateAt(RedstoneFace::relative(position, face), BlockUpdateType::Normal, &position);
    }
}

// a block of the plain class, with none of the names handled apart below: no update does anything to
// it. Dirt, stone and the like, which is nearly every neighbour of a block an explosion breaks
bool BlockUpdates::ignores(const BlockState &state) {
    const Block *block = VanillaBlocks::fromState(state);
    if (block == nullptr || !block->isPlainClass())
        return false;

    // air and one kind of ground make nearly all of them: the last two answers come first
    static const Block *recent[2] = {nullptr, nullptr};
    static bool recentAnswer[2] = {false, false};
    if (block == recent[0])
        return recentAnswer[0];
    if (block == recent[1])
        return recentAnswer[1];

    static std::unordered_map<const Block *, bool> known;
    auto found = known.find(block);
    if (found == known.end()) {
        const std::string &identifier = state.mName;
        found = known.emplace(block, !PistonSystem::isPiston(identifier) && !FireSystem::matches(identifier)
                                             && !FallingBlockSystem::matches(identifier)
                                             && !CommandBlock::matches(identifier)
                                             && !DetectorSystem::isWire(state))
                        .first;
    }
    recent[1] = recent[0];
    recentAnswer[1] = recentAnswer[0];
    recent[0] = block;
    recentAnswer[0] = found->second;
    return found->second;
}

void Level::updateAt(const Vector3i &position, BlockUpdateType type, const Vector3i *changed) {
    if (mOwner == nullptr || !isChunkReady(*this, position))
        return;

    EventSignal<BlockUpdateBeforeEvent> &blockUpdate = mOwner->getEventBus().before().mBlockUpdate;
    const bool notify = type == BlockUpdateType::Normal && changed != nullptr && blockUpdate.hasSubscribers();
    if (!notify && type != BlockUpdateType::Scheduled) {
        const BlockState *inPlace = peekBlockPtr(position.x, position.y, position.z);
        if (inPlace != nullptr && BlockUpdates::ignores(*inPlace))
            return;
    }

    if (mUpdateDepth >= MAX_UPDATE_DEPTH) {
        if (type != BlockUpdateType::Scheduled)
            scheduleUpdate(position, 1);
        return;
    }

    ++mUpdateDepth;
    if (notify) {
        BlockUpdateBeforeEvent event(*this, position);
        blockUpdate.emit(event);
        const BlockState *inPlace = peekBlockPtr(position.x, position.y, position.z);
        if (event.isCancelled() || (inPlace != nullptr && BlockUpdates::ignores(*inPlace))) {
            --mUpdateDepth;
            return;
        }
    }

    ServerNetworkHandler &owner = *mOwner;
    // the state the chunk holds lives as long as the process: read in place, copied only to be reshaped
    const BlockState *held = peekBlockPtr(position.x, position.y, position.z);
    BlockState reshaped;
    if (held == nullptr) {
        reshaped = getBlockState(position.x, position.y, position.z);
        held = &reshaped;
    }
    const Block *shapedBlock = VanillaBlocks::fromState(*held);
    if (shapedBlock != nullptr && shapedBlock->hasDynamicShape()) {
        const BlockState shaped = shapedBlock->updateShape(*this, position, *held);
        if (!(shaped == *held)) {
            setBlock(position, shaped, false);
            reshaped = shaped;
            held = &reshaped;
        }
    }
    const BlockState &state = *held;
    const std::string &identifier = state.mName;

    // which of the systems below a name belongs to, asked once per name
    enum : uint8_t { OTHER, PISTON, FIRE, FALLING, COMMAND };
    static std::unordered_map<std::string, uint8_t> systems;
    auto system = systems.find(identifier);
    if (system == systems.end())
        system = systems.emplace(identifier, PistonSystem::isPiston(identifier)          ? PISTON
                                             : FireSystem::matches(identifier)          ? FIRE
                                             : FallingBlockSystem::matches(identifier)  ? FALLING
                                             : CommandBlock::matches(identifier)        ? COMMAND
                                                                                         : OTHER)
                         .first;

    if (system->second == PISTON) {
        if (type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone)
            PistonSystem::onRedstoneUpdate(owner, *this, position, state);
    } else if (system->second == FIRE) {
        if (type == BlockUpdateType::Normal)
            FireSystem::onNormalUpdate(owner, *this, position, state);
    } else if (system->second == FALLING) {
        if (type == BlockUpdateType::Normal)
            FallingBlockSystem::onNormalUpdate(owner, *this, position, state, changed);
        else if (type == BlockUpdateType::Scheduled)
            FallingBlockSystem::onScheduledUpdate(owner, *this, position, state);
    } else if (system->second == COMMAND) {
        if (type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone)
            CommandBlockSystem::setPowered(owner, *this, position,
                                           RedstoneSystem::isGettingPower(owner, *this, position));
    } else {
        RedstoneSystem::onRedstoneUpdate(owner, *this, position, state, type);
        const CustomBlockDefinition *custom = shapedBlock == nullptr && type != BlockUpdateType::Scheduled
                                                      ? CustomContentRegistry::getInstance().findBlock(identifier)
                                                      : nullptr;
        if (custom != nullptr && custom->mRedstoneConsumer)
            owner.getScriptEngine().onCustomBlockRedstone(
                    *this, identifier, position,
                    RedstoneSystem::isBlockIndirectlyGettingPowered(owner, *this, position));
    }

    if (type == BlockUpdateType::Normal || type == BlockUpdateType::Scheduled) {
        const BlockState *inPlaceNow = peekBlockPtr(position.x, position.y, position.z);
        BlockState fetched;
        if (inPlaceNow == nullptr) {
            fetched = getBlockState(position.x, position.y, position.z);
            inPlaceNow = &fetched;
        }
        const BlockState &current = *inPlaceNow;
        const Block *block = VanillaBlocks::fromState(current);
        if (block != nullptr && type == BlockUpdateType::Normal) {
            if (block->watchesNeighbour(position, current, changed))
                block->onNeighbourChanged(owner, *this, position, current);
        } else if (block == nullptr && type == BlockUpdateType::Normal) {
            // a pack block has no class: its placement filter is its survival
            const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(current.mName);
            if (custom != nullptr && !custom->mPlacementFilter.survives(*this, position))
                BlockActionHandler::destroyBlock(owner, *this, position, current, true, ItemStack::air());
        } else if (block != nullptr)
            block->onScheduledUpdate(owner, *this, position, current);
        else if (type == BlockUpdateType::Scheduled
                 && CustomContentRegistry::getInstance().isCustomBlock(current.mName))
            owner.getScriptEngine().onCustomBlockTick(*this, current.mName, position);
    }

    --mUpdateDepth;
}

void Level::scheduleUpdate(const Vector3i &position, int64_t delay) {
    mBlockUpdates.schedule(position, delay < 1 ? 1 : delay);
}

bool Level::isUpdateScheduled(const Vector3i &position) const {
    return mBlockUpdates.isScheduled(position);
}

void Level::cancelUpdate(const Vector3i &position) {
    mBlockUpdates.cancel(position);
}

void Level::tickBlockUpdates() {
    mBlockUpdates.tick([this](const Vector3i &position) { updateAt(position, BlockUpdateType::Scheduled); },
                       [this](int32_t chunkX, int32_t chunkZ) { return isColumnActive(chunkX, chunkZ); });
    _tickNamedTicks();
    _tickNeighborChanges();
}

void Level::_recordNeighborChange(int32_t x, int32_t y, int32_t z, const BlockState &previous,
                                  const BlockState &current) {
    const CustomContentRegistry &custom = CustomContentRegistry::getInstance();
    const Vector3i changed(x, y, z);
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        const Vector3i neighbor = RedstoneFace::relative(changed, face);
        const BlockState *listener = peekBlockPtr(neighbor.x, neighbor.y, neighbor.z);
        if (listener == nullptr)
            continue;
        const int direction = RedstoneFace::opposite(face);
        if ((custom.neighborDirectionsOf(listener->mName) & (1 << direction)) != 0)
            mNeighborChanges.record(neighbor, listener->mName, direction, previous, current);
    }
}

// changes made while the events run wait for the next tick
void Level::_tickNeighborChanges() {
    if (mOwner == nullptr || mNeighborChanges.empty())
        return;

    ServerNetworkHandler &owner = *mOwner;
    for (const NeighborChangeQueue::Entry &entry: mNeighborChanges.take()) {
        const BlockState *state = peekBlockPtr(entry.position.x, entry.position.y, entry.position.z);
        if (state == nullptr || state->mName != entry.blockType)
            continue;
        const BlockState listener = *state;
        owner.getScriptEngine().onCustomBlockNeighborChanged(*this, entry.position, listener, entry.changes);
    }
}

void Level::_tickNamedTicks() {
    if (mOwner == nullptr || !mNamedTicks.hasColumns())
        return;

    ServerNetworkHandler &owner = *mOwner;
    const int64_t levelTick = owner.getCurrentTick();
    for (const std::pair<int32_t, int32_t> &column: mNamedTicks.getColumns()) {
        if (!isColumnActive(column.first, column.second))
            continue;
        mNamedTicks.tickColumn(column.first, column.second, levelTick,
                               [&](const NamedTickQueue::Entry &entry, const std::string &eventNamespace) {
                                   // the event belongs to the block type it was scheduled on
                                   const BlockState state = getBlockState(entry.position.x, entry.position.y,
                                                                          entry.position.z);
                                   if (state.mName != entry.blockType || state.mName == BlockNames::AIR)
                                       return;
                                   owner.getScriptEngine().onCustomBlockNamedTick(*this, entry.position, state,
                                                                                  eventNamespace, entry.eventName);
                               });
    }
}

void Level::onBlockPlaced(const Vector3i &position, const BlockState &state) {
    if (mOwner == nullptr)
        return;

    ServerNetworkHandler &owner = *mOwner;
    const Block *placed = VanillaBlocks::fromState(state);
    if (placed != nullptr)
        placed->onPlacedInLevel(owner, *this, position, state);

    if (FallingBlockSystem::matches(state.mName)) {
        updateAround(position);
        RedstoneSystem::updateAroundRedstone(owner, *this, position);
        FallingBlockSystem::onBlockPlaced(owner, *this, position, state);
        return;
    }

    RedstoneSystem::onRedstonePlaced(owner, *this, position, state);
}

void Level::onBlockBroken(const Vector3i &position, const BlockState &previous) {
    if (mOwner == nullptr)
        return;

    cancelUpdate(position);
    RedstoneSystem::onRedstoneBroken(*mOwner, *this, position, previous);
}
