#include "Level/Level.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/CommandBlockSystem.h"
#include "Block/Systems/FallingBlockSystem.h"
#include "Block/Systems/FireSystem.h"
#include "Block/Systems/PistonSystem.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Block/Blocks/CommandBlock.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

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

    setBlockState(position.x, position.y, position.z, state);

    if (mOwner != nullptr)
        BlockActionHandler::broadcastBlockUpdate(*mOwner, *this, position, state);

    if (!update)
        return;

    updateAt(position, BlockUpdateType::Normal);
    updateAround(position);
}

void Level::updateAround(const Vector3i &position) {
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        updateAt(RedstoneFace::relative(position, face), BlockUpdateType::Normal);
    }
}

void Level::updateAt(const Vector3i &position, BlockUpdateType type) {
    if (mOwner == nullptr || !isChunkReady(*this, position))
        return;

    if (mUpdateDepth >= MAX_UPDATE_DEPTH) {
        if (type != BlockUpdateType::Scheduled)
            scheduleUpdate(position, 1);
        return;
    }

    ++mUpdateDepth;

    ServerNetworkHandler &owner = *mOwner;
    const BlockState state = getBlockState(position.x, position.y, position.z);
    const std::string &identifier = state.mName;

    if (PistonSystem::isPiston(identifier)) {
        if (type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone)
            PistonSystem::onRedstoneUpdate(owner, *this, position, state);
    } else if (FireSystem::matches(identifier)) {
        if (type == BlockUpdateType::Normal)
            FireSystem::onNormalUpdate(owner, *this, position, state);
    } else if (FallingBlockSystem::matches(identifier)) {
        if (type == BlockUpdateType::Normal)
            FallingBlockSystem::onNormalUpdate(owner, *this, position, state);
    } else if (CommandBlock::matches(identifier)) {
        if (type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone)
            CommandBlockSystem::setPowered(owner, *this, position,
                                           RedstoneSystem::isGettingPower(owner, *this, position));
    } else {
        RedstoneSystem::onRedstoneUpdate(owner, *this, position, state, type);
    }

    if (type == BlockUpdateType::Normal) {
        const BlockState current = getBlockState(position.x, position.y, position.z);
        const Block *block = VanillaBlocks::fromIdentifier(current.mName);
        if (block != nullptr)
            block->onNeighbourChanged(owner, *this, position, current);
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
    mBlockUpdates.tick([this](const Vector3i &position) {
                           updateAt(position, BlockUpdateType::Scheduled);
                       },
                       [this](int32_t chunkX, int32_t chunkZ) {
                           return isColumnActive(chunkX, chunkZ);
                       });
}

void Level::onBlockPlaced(const Vector3i &position, const BlockState &state) {
    if (mOwner == nullptr)
        return;

    ServerNetworkHandler &owner = *mOwner;

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
