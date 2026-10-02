#include "Block/Systems/FallingBlockSystem.h"

#include "Actor/Misc/FallingBlock.h"
#include "Block/BlockData.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"

#include <unordered_set>

namespace {
    const char *FALLABLE_BLOCKS[] = {
            "minecraft:sand",
            "minecraft:red_sand",
            "minecraft:gravel",
            "minecraft:suspicious_sand",
            "minecraft:suspicious_gravel",
            "minecraft:anvil",
            "minecraft:chipped_anvil",
            "minecraft:damaged_anvil",
            "minecraft:deprecated_anvil",
            "minecraft:dragon_egg",
            "minecraft:scaffolding",
            "minecraft:pointed_dripstone",
            "minecraft:white_concrete_powder",
            "minecraft:orange_concrete_powder",
            "minecraft:magenta_concrete_powder",
            "minecraft:light_blue_concrete_powder",
            "minecraft:yellow_concrete_powder",
            "minecraft:lime_concrete_powder",
            "minecraft:pink_concrete_powder",
            "minecraft:gray_concrete_powder",
            "minecraft:light_gray_concrete_powder",
            "minecraft:cyan_concrete_powder",
            "minecraft:purple_concrete_powder",
            "minecraft:blue_concrete_powder",
            "minecraft:brown_concrete_powder",
            "minecraft:green_concrete_powder",
            "minecraft:red_concrete_powder",
            "minecraft:black_concrete_powder"
    };

    const std::string CONCRETE_POWDER_SUFFIX = "_concrete_powder";

    const std::unordered_set<std::string> &fallableSet() {
        static const std::unordered_set<std::string> blocks(std::begin(FALLABLE_BLOCKS),
                                                            std::end(FALLABLE_BLOCKS));
        return blocks;
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool isChunkReady(Level &level, const Vector3i &position) {
        if (position.y < level.getMinY() || position.y > level.getMaxY())
            return false;

        return level.isChunkResident(position.x >> 4, position.z >> 4);
    }
}

bool FallingBlockSystem::matches(const std::string &identifier) {
    return fallableSet().count(identifier) != 0;
}

bool FallingBlockSystem::breaksOnLava(const std::string &identifier) {
    return identifier == "minecraft:scaffolding";
}

bool FallingBlockSystem::breaksOnGround(const std::string &identifier) {
    return identifier == "minecraft:suspicious_sand" || identifier == "minecraft:suspicious_gravel" ||
           identifier == "minecraft:pointed_dripstone";
}

bool FallingBlockSystem::isConcretePowder(const std::string &identifier) {
    return identifier.size() > CONCRETE_POWDER_SUFFIX.size() &&
           identifier.compare(identifier.size() - CONCRETE_POWDER_SUFFIX.size(),
                              CONCRETE_POWDER_SUFFIX.size(), CONCRETE_POWDER_SUFFIX) == 0;
}

std::string FallingBlockSystem::getConcreteFor(const std::string &identifier) {
    if (!isConcretePowder(identifier))
        return std::string();

    return identifier.substr(0, identifier.size() - std::string("_powder").size());
}

std::string FallingBlockSystem::getNextAnvilDamage(const std::string &identifier) {
    if (identifier == "minecraft:anvil")
        return "minecraft:chipped_anvil";

    if (identifier == "minecraft:chipped_anvil")
        return "minecraft:damaged_anvil";

    return std::string();
}

bool FallingBlockSystem::isWater(const std::string &identifier) {
    return identifier == "minecraft:water" || identifier == "minecraft:flowing_water";
}

bool FallingBlockSystem::isLava(const std::string &identifier) {
    return identifier == "minecraft:lava" || identifier == "minecraft:flowing_lava";
}

bool FallingBlockSystem::isTransparent(const BlockState &state) {
    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    return data != nullptr && data->mTransparent;
}

bool FallingBlockSystem::canFallInto(Level &level, const Vector3i &position) {
    if (position.y < level.getMinY())
        return false;

    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    const std::string &identifier = state.mName;

    if (identifier == "minecraft:air" || identifier == "minecraft:fire" || identifier == "minecraft:soul_fire")
        return true;

    if (isWater(identifier) || isLava(identifier))
        return true;

    if (identifier == "minecraft:bubble_column") {
        const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
        return isWater(below.mName) || isLava(below.mName);
    }

    return false;
}

bool FallingBlockSystem::isTouchingWater(Level &level, const Vector3i &position) {
    static const Vector3i OFFSETS[] = {
            Vector3i(0, 1, 0),
            Vector3i(0, 0, -1),
            Vector3i(0, 0, 1),
            Vector3i(-1, 0, 0),
            Vector3i(1, 0, 0)
    };

    for (const Vector3i &offset: OFFSETS) {
        const BlockState side = level.getBlockState(position.x + offset.x, position.y + offset.y,
                                                    position.z + offset.z);
        if (isWater(side.mName))
            return true;
    }

    return false;
}

void FallingBlockSystem::setBlockState(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) {
    if (!isChunkReady(level, position))
        return;

    level.setBlockState(position.x, position.y, position.z, state);

    UpdateBlockPacket update;
    update.mBlockPosition = position;
    update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = 0;

    BlockActionHandler::broadcastToViewers(owner, level, centerOf(position), update);
}

void FallingBlockSystem::spawnDestroyParticle(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                              const BlockState &state) {
    owner.broadcastLevelEvent(level, LevelEventPacket::Event::ParticleDestroy, centerOf(position),
                              (int32_t) BlockStateHasher::hash(state.mName, state.mStates));
}

void FallingBlockSystem::spawnFallingBlock(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                           const BlockState &state) {
    if (!isChunkReady(level, position))
        return;

    setBlockState(owner, level, position, BlockState("minecraft:air"));

    const Vector3f spawnPosition((float) position.x + 0.5f, (float) position.y, (float) position.z + 0.5f);

    FallingBlock *actor = owner.spawnFallingBlock(level, state, spawnPosition);
    if (actor == nullptr)
        return;

    actor->setBreakOnLava(breaksOnLava(state.mName));
    actor->setBreakOnGround(breaksOnGround(state.mName));

    level.onBlockBroken(position, state);
}

void FallingBlockSystem::onNormalUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                        const BlockState &state) {
    if (!isChunkReady(level, position))
        return;

    if (isConcretePowder(state.mName) && isTouchingWater(level, position)) {
        const BlockState concrete(getConcreteFor(state.mName));
        if (BlockChangeSystem::allows(level, position, concrete, BlockChangeCause::Form))
            setBlockState(owner, level, position, concrete);
        return;
    }

    if (state.mName == "minecraft:pointed_dripstone" && state.mStates.getBool("hanging", false)) {
        const BlockState above = level.getBlockState(position.x, position.y + 1, position.z);
        if (level.isSolidAt(position.x, position.y + 1, position.z) || above.mName == "minecraft:pointed_dripstone")
            return;
    }

    const Vector3i below(position.x, position.y - 1, position.z);
    if (!canFallInto(level, below))
        return;

    spawnFallingBlock(owner, level, position, state);
}

void FallingBlockSystem::onBlockPlaced(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) {
    onNormalUpdate(owner, level, position, state);
}
