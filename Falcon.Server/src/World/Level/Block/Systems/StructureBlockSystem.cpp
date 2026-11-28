#include "World/Level/Block/Systems/StructureBlockSystem.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/StructureBlockUpdatePacket.h"
#include "Protocol/Packets/StructureTemplateDataRequestPacket.h"
#include "Protocol/Packets/StructureTemplateDataResponsePacket.h"
#include "Scripting/ScriptStructureRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/StructureBlockActor.h"
#include "World/Level/Block/StructureBlock.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace {
    constexpr int32_t MAX_HORIZONTAL = McStructure::MAX_HORIZONTAL_SIZE;
    constexpr int32_t MAX_HEIGHT = 257;
    constexpr int32_t WORLD_LIMIT = 30000000;
    constexpr float MAX_ANIMATION_SECONDS = 300.0f;
    constexpr std::array<const char *, 6> MODES{"data", "save", "load", "corner", "invalid", "export"};

    bool validName(const std::string &name) {
        return name.size() <= 128 && ScriptStructureRegistry::isValidIdentifier(name)
               && std::all_of(name.begin(), name.end(), [](char c) {
                      return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ':'
                             || c == '/' || c == '.';
                  });
    }

    bool validSettings(const StructureSettings &settings) {
        const auto &size = settings.mSize;
        return size.x >= 0 && size.x <= MAX_HORIZONTAL && size.y >= 0 && size.y <= MAX_HEIGHT && size.z >= 0
               && size.z <= MAX_HORIZONTAL && (unsigned) settings.mRotation <= 3 && (unsigned) settings.mMirror <= 3
               && (unsigned) settings.mAnimationMode <= 2 && std::isfinite(settings.mAnimationSeconds)
               && settings.mAnimationSeconds >= 0 && settings.mAnimationSeconds <= MAX_ANIMATION_SECONDS
               && std::isfinite(settings.mIntegrityValue) && settings.mIntegrityValue >= 0
               && settings.mIntegrityValue <= 100;
    }

    bool bounds(Level &level, const Vector3i &origin, const Vector3i &offset, const Vector3i &size, Vector3i &from) {
        const int64_t x = (int64_t) origin.x + offset.x;
        const int64_t y = (int64_t) origin.y + offset.y;
        const int64_t z = (int64_t) origin.z + offset.z;
        if (size.x <= 0 || size.x > MAX_HORIZONTAL || size.y <= 0 || size.y > MAX_HEIGHT
            || size.z <= 0 || size.z > MAX_HORIZONTAL || x < -WORLD_LIMIT || x + size.x - 1 > WORLD_LIMIT
            || z < -WORLD_LIMIT || z + size.z - 1 > WORLD_LIMIT || y < level.getMinY()
            || y + size.y - 1 > level.getMaxY())
            return false;

        from = Vector3i((int32_t) x, (int32_t) y, (int32_t) z);
        for (int32_t chunkX = from.x >> 4; chunkX <= ((from.x + size.x - 1) >> 4); ++chunkX) {
            for (int32_t chunkZ = from.z >> 4; chunkZ <= ((from.z + size.z - 1) >> 4); ++chunkZ) {
                if (!level.isChunkResident(chunkX, chunkZ))
                    return false;
            }
        }
        return true;
    }

    bool isStructureBlock(Level &level, const Vector3i &at) {
        const auto *state = level.peekBlockPtr(at.x, at.y, at.z);
        return state != nullptr && StructureBlock::matches(state->mName);
    }

    bool detectCorners(Level &level, StructureBlockActor &actor) {
        const Vector3i at = actor.getPosition();
        const std::string name = ScriptStructureRegistry::normalize(actor.mStructureName);
        Vector3i minimum, maximum;
        size_t count = 0;
        for (const auto *corner: level.getBlockActors().findAll<StructureBlockActor>()) {
            const Vector3i pos = corner->getPosition();
            if (corner->mMode != StructureBlockActor::Mode::Corner
                || ScriptStructureRegistry::normalize(corner->mStructureName) != name || !isStructureBlock(level, pos))
                continue;
            if (count++ == 0) {
                minimum = maximum = pos;
                continue;
            }
            minimum = Vector3i(std::min(minimum.x, pos.x), std::min(minimum.y, pos.y), std::min(minimum.z, pos.z));
            maximum = Vector3i(std::max(maximum.x, pos.x), std::max(maximum.y, pos.y), std::max(maximum.z, pos.z));
        }
        const Vector3i size(maximum.x - minimum.x - 1, maximum.y - minimum.y - 1, maximum.z - minimum.z - 1);
        const Vector3i offset(minimum.x + 1 - at.x, minimum.y + 1 - at.y, minimum.z + 1 - at.z);
        Vector3i from;
        if (count < 2 || !bounds(level, at, offset, size, from))
            return false;
        actor.mOffset = offset;
        actor.mSize = size;
        return true;
    }

    void sendData(ServerNetworkHandler &owner, ServerPlayer &player, const StructureTemplateDataResponsePacket &response) {
        owner.sendPacketTo(player.getNetworkIdentifier(), response);
    }
}

StructureBlockSystem::StructureBlockSystem() = default;
StructureBlockSystem::~StructureBlockSystem() = default;

bool StructureBlockSystem::canEdit(ServerPlayer &player, const Vector3i &position) {
    return player.isSpawned() && !player.isDead() && player.isCreative() && player.isOp()
           && position.x >= -WORLD_LIMIT && position.x <= WORLD_LIMIT && position.z >= -WORLD_LIMIT
           && position.z <= WORLD_LIMIT && BlockActionHandler::canInteractWithBlock(player, position);
}

ScriptStructureRegistry &StructureBlockSystem::_registry(ServerNetworkHandler &owner, Level &level) {
    World &world = owner.getWorldOf(level);
    if (&world == &owner.getWorlds().getDefault()) {
        auto &registry = owner.getScriptEngine().structures();
        registry.setStorage(&world.getOverworld());
        return registry;
    }
    if (mRegistry == nullptr) {
        mRegistry = std::make_unique<ScriptStructureRegistry>();
        mRegistry->setStorage(&world.getOverworld());
    }
    return *mRegistry;
}

std::shared_ptr<McStructure> StructureBlockSystem::_find(ServerNetworkHandler &owner, Level &level,
                                                        const std::string &name) {
    auto structure = _registry(owner, level).find(name);
    return structure != nullptr ? structure : owner.getScriptEngine().structures().findPack(name);
}

void StructureBlockSystem::update(ServerNetworkHandler &owner, ServerPlayer &player,
                                  const StructureBlockUpdatePacket &packet) {
    const Vector3i &at = packet.mBlockPosition;
    if (!canEdit(player, at))
        return;
    Level &level = owner.getLevelFor(player);
    if (!isStructureBlock(level, at))
        return;

    const auto &edit = packet.mEditorData;
    const auto &settings = edit.mSettings;
    const auto mode = (unsigned) edit.mType;
    if (mode >= MODES.size() || edit.mType == StructureBlockType::Invalid
        || (unsigned) edit.mRedstoneSaveMode > 1 || !validSettings(settings)
        || (!edit.mName.empty() && !validName(edit.mName)) || edit.mFilteredName.size() > 128
        || edit.mDataField.size() > 512)
        return;

    auto &actor = level.getBlockActors().getOrCreate<StructureBlockActor>(at);
    actor.mMode = (StructureBlockActor::Mode) edit.mType;
    actor.mStructureName = edit.mName.empty() ? std::string() : ScriptStructureRegistry::normalize(edit.mName);
    actor.mHasFilteredName = !edit.mFilteredName.empty();
    actor.mFilteredName = edit.mFilteredName;
    actor.mDataField = edit.mDataField;
    actor.mOffset = settings.mOffset;
    actor.mSize = settings.mSize;
    actor.mLastTouchedPlayerId = player.getUniqueId();
    actor.mIncludePlayers = edit.mIncludingPlayers;
    actor.mShowBoundingBox = edit.mBoundingBoxVisible && edit.mType != StructureBlockType::Corner;
    actor.mIgnoreEntities = settings.mIgnoringEntities;
    actor.mRemoveBlocks = settings.mIgnoringBlocks;
    actor.mRedstoneSaveMode = (int32_t) edit.mRedstoneSaveMode;
    actor.mRotation = (uint8_t) settings.mRotation;
    actor.mMirror = (uint8_t) settings.mMirror;
    actor.mAnimationMode = (uint8_t) settings.mAnimationMode;
    actor.mAnimationSeconds = settings.mAnimationSeconds;
    actor.mIntegrity = settings.mIntegrityValue;
    actor.mSeed = settings.mIntegritySeed;

    if (actor.mMode == StructureBlockActor::Mode::Save && !actor.mStructureName.empty()
        && (actor.mSize.x == 0 || actor.mSize.y == 0 || actor.mSize.z == 0))
        detectCorners(level, actor);

    BlockState state = level.getBlockState(at.x, at.y, at.z);
    if (state.mStates.getString("structure_block_type") != MODES[mode]) {
        state.mStates.putString("structure_block_type", MODES[mode]);
        level.setBlock(at, state, true);
    }
    auto *current = level.getBlockActors().find<StructureBlockActor>(at);
    if (current == nullptr)
        return;
    BlockActionHandler::broadcastBlockActorData(owner, level, *current);
    if (packet.mTrigger)
        activate(owner, level, *current, true, packet.mWaterlogged);
}

void StructureBlockSystem::request(ServerNetworkHandler &owner, ServerPlayer &player,
                                   const StructureTemplateDataRequestPacket &packet) {
    const auto &at = packet.mPosition;
    if (!canEdit(player, at))
        return;
    Level &level = owner.getLevelFor(player);
    if (!isStructureBlock(level, at) || !validSettings(packet.mSettings) || !validName(packet.mName))
        return;

    using Operation = StructureTemplateDataRequestPacket::Operation;
    using Response = StructureTemplateDataResponsePacket;
    Response response;
    response.mName = packet.mName;
    if (packet.mOperation == Operation::QuerySavedStructure || packet.mOperation == Operation::ExportFromLoadMode) {
        response.mType = packet.mOperation == Operation::QuerySavedStructure ? Response::Type::Query : Response::Type::Export;
        if (auto structure = _find(owner, level, packet.mName); structure != nullptr) {
            response.mTag = structure->toNbt();
            response.mSave = true;
        }
    } else if (packet.mOperation == Operation::ExportFromSavedMode) {
        response.mType = Response::Type::Export;
        Vector3i from;
        const auto &settings = packet.mSettings;
        if (bounds(level, at, settings.mOffset, settings.mSize, from)) {
            const Vector3i to(from.x + settings.mSize.x - 1, from.y + settings.mSize.y - 1,
                              from.z + settings.mSize.z - 1);
            response.mTag = McStructure::capture(owner, level, from, to, !settings.mIgnoringBlocks,
                                                 !settings.mIgnoringEntities).toNbt();
            response.mSave = true;
        }
    } else {
        return;
    }
    sendData(owner, player, response);
}

void StructureBlockSystem::activate(ServerNetworkHandler &owner, Level &level, const StructureBlockActor &actor,
                                    bool saveToDisk, bool waterlogged) {
    if (!validName(actor.mStructureName))
        return;
    const auto at = actor.getPosition();
    Vector3i from;
    if (actor.mMode == StructureBlockActor::Mode::Save) {
        if (!bounds(level, at, actor.mOffset, actor.mSize, from))
            return;
        const Vector3i to(from.x + actor.mSize.x - 1, from.y + actor.mSize.y - 1, from.z + actor.mSize.z - 1);
        auto structure = std::make_shared<McStructure>(McStructure::capture(owner, level, from, to,
                                                                             !actor.mRemoveBlocks, !actor.mIgnoreEntities));
        _registry(owner, level).store(actor.mStructureName, std::move(structure), saveToDisk);
        return;
    }
    if (actor.mMode != StructureBlockActor::Mode::Load || actor.mRotation > 3 || actor.mMirror > 3)
        return;
    auto structure = _find(owner, level, actor.mStructureName);
    if (structure == nullptr || !bounds(level, at, actor.mOffset, structure->rotatedSize(actor.mRotation), from))
        return;
    McStructure::PlaceOptions options;
    options.mBlocks = !actor.mRemoveBlocks;
    options.mEntities = !actor.mIgnoreEntities;
    options.mWaterlogged = waterlogged;
    options.mRotation = actor.mRotation;
    options.mMirror = (McStructure::Mirror) actor.mMirror;
    options.mIntegrity = actor.mIntegrity / 100.0f;
    options.mSeed = actor.mSeed == 0 ? std::random_device{}() : (uint32_t) actor.mSeed;
    if (!options.mBlocks || actor.mAnimationMode == 0 || actor.mAnimationMode > 2
        || !std::isfinite(actor.mAnimationSeconds) || actor.mAnimationSeconds <= 0) {
        structure->place(owner, level, from, options);
        return;
    }
    const int64_t ticks = (int64_t) std::ceil(std::min(actor.mAnimationSeconds, MAX_ANIMATION_SECONDS) * 20.0f);
    const auto &size = structure->getSize();
    const int64_t layerCells = (int64_t) size.x * size.z;
    const int64_t cellsPerUnit = actor.mAnimationMode == (uint8_t) StructureAnimationMode::Layer ? layerCells : 1;
    mPending.push_back({&level, std::move(structure), std::move(options), from, ticks, 0,
                         layerCells * size.y / cellsPerUnit, cellsPerUnit, {}});
}

void StructureBlockSystem::tick(ServerNetworkHandler &owner, Level &level) {
    if (mPending.empty())
        return;
    // applying a structure can schedule another placement
    auto pending = std::move(mPending);
    mPending.clear();
    for (auto &job: pending) {
        if (job.mLevel != &level) {
            mPending.push_back(std::move(job));
            continue;
        }
        Vector3i from;
        if (!bounds(level, job.mPosition, Vector3i(0, 0, 0), job.mStructure->rotatedSize(job.mOptions.mRotation), from)) {
            mPending.push_back(std::move(job));
            continue;
        }
        auto options = job.mOptions;
        options.mFromCell = (job.mUnits * job.mTick / job.mTicks) * job.mCellsPerUnit;
        ++job.mTick;
        options.mToCell = (job.mUnits * job.mTick / job.mTicks) * job.mCellsPerUnit;
        options.mPendingBlockUpdates = &job.mPendingBlockUpdates;
        options.mFinishBlockUpdates = job.mTick == job.mTicks;
        options.mEntities = options.mEntities && options.mFinishBlockUpdates;
        if (options.mFromCell != options.mToCell || options.mFinishBlockUpdates)
            job.mStructure->place(owner, level, from, options);
        if (job.mTick < job.mTicks)
            mPending.push_back(std::move(job));
    }
}
