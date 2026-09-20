#include "Actor/FallingBlockActor.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/BlockSupport.h"
#include "Block/Systems/FallingBlockSystem.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

const char *FallingBlockActor::IDENTIFIER = "minecraft:falling_block";

const float FallingBlockActor::GRAVITY = 0.04f;
const float FallingBlockActor::DRAG = 0.02f;
const float FallingBlockActor::BASE_OFFSET = 0.49f;
const float FallingBlockActor::ANVIL_DAMAGE_PER_FALL_DISTANCE = 2.0f;
const float FallingBlockActor::ANVIL_MAX_DAMAGE = 40.0f;
const float FallingBlockActor::ANVIL_LAND_VOLUME = 1.0f;
const float FallingBlockActor::ANVIL_LAND_PITCH = 0.8f;

namespace {
    const int32_t ACTOR_DATA_VARIANT = 2;
    const char *ANVIL_LAND_SOUND = "random.anvil_land";
    const char *DRIPSTONE_LAND_SOUND = "pointed_dripstone.land";
    const char *ANVIL_DEATH_KEY = "death.attack.anvil";
    const char *DRIPSTONE_DEATH_KEY = "death.attack.stalagmite";
}

FallingBlockActor::FallingBlockActor(uint64_t runtimeId, const BlockState &blockState)
        : ServerActor(runtimeId, IDENTIFIER), mBlockState(blockState) {
}

bool FallingBlockActor::_hasAliveLimit() const {
    const std::string &identifier = mBlockState.mName;

    return identifier == "minecraft:sand" || identifier == "minecraft:gravel" ||
           identifier == "minecraft:anvil";
}

Vector3i FallingBlockActor::_restingPosition() const {
    const Vector3f position = getPosition();

    return Vector3i((int32_t) std::lround(position.x - 0.5f),
                    (int32_t) std::lround(position.y),
                    (int32_t) std::lround(position.z - 0.5f));
}

void FallingBlockActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    EntityDataEntry variant;
    variant.mId = ACTOR_DATA_VARIANT;
    variant.mFormat = EntityDataFormat::Int;
    variant.mIntValue = mBlockState.getHash();
    metadata.mEntries.push_back(variant);
}

void FallingBlockActor::tick(ServerNetworkHandler &owner) {
    if (mExpired)
        return;

    Level &level = owner.getLevelFor(*this);
    const Vector3i restingPosition = _restingPosition();

    if (!level.isChunkResident(restingPosition.x >> 4, restingPosition.z >> 4))
        return;

    if (_hasAliveLimit()) {
        ++mAliveTicks;
        if (mAliveTicks > ALIVE_TICK_LIMIT) {
            _destroy(owner, restingPosition, true);
            return;
        }
    }

    Vector3f motion = getMotion();
    motion.y -= GRAVITY;

    Vector3f position = getPosition();
    position.x += motion.x;
    position.y += motion.y;
    position.z += motion.z;

    const float friction = 1.0f - DRAG;
    motion.x *= friction;
    motion.y *= friction;
    motion.z *= friction;

    if (position.y < (float) level.getMinY()) {
        mExpired = true;
        return;
    }

    const int32_t blockX = (int32_t) std::lround(position.x - 0.5f);
    const int32_t blockZ = (int32_t) std::lround(position.z - 0.5f);
    const int32_t feetY = (int32_t) std::floor(position.y);

    bool landed = false;
    if (!FallingBlockSystem::canFallInto(level, Vector3i(blockX, feetY, blockZ))) {
        position.y = (float) (feetY + 1);
        motion.y = 0.0f;
        landed = true;
    }

    setMotion(motion);
    setPosition(position);
    setOnGround(landed);

    if (position.y > getHighestPosition())
        setHighestPosition(position.y);

    updateFallDistance();

    const Vector3i currentPosition = _restingPosition();

    if (mBreakOnLava) {
        const BlockState below = level.getBlockState(currentPosition.x, currentPosition.y - 1,
                                                     currentPosition.z);
        if (FallingBlockSystem::isLava(below.mName)) {
            _destroy(owner, currentPosition, true);
            return;
        }
    }

    if (landed) {
        _land(owner, currentPosition);
        return;
    }

    owner.broadcastActorMove(*this);
}

void FallingBlockActor::_destroy(ServerNetworkHandler &owner, const Vector3i &position, bool dropItem) {
    mExpired = true;

    FallingBlockSystem::spawnDestroyParticle(owner, owner.getLevelFor(*this), position, mBlockState);

    if (dropItem)
        _dropItem(owner);
}

void FallingBlockActor::_dropItem(ServerNetworkHandler &owner) {
    const char *rule = mBlockState.mName == "minecraft:snow_layer" ? "dotiledrops" : "doentitydrops";
    if (!owner.getLevel().getGameRules().getBool(rule))
        return;

    const Vector3f position = getPosition();

    std::string identifier = mBlockState.mName;
    const BlockData *data = BlockDataTable::find(identifier.c_str());
    if (data != nullptr && data->mDropKind == BlockDropKind::Other && data->mDropIdentifier != nullptr &&
        data->mDropIdentifier[0] != '\0')
        identifier = data->mDropIdentifier;

    owner.spawnItemActor(owner.getLevelFor(*this), identifier, 1, position);
}

void FallingBlockActor::_land(ServerNetworkHandler &owner, const Vector3i &position) {
    mExpired = true;

    Level &level = owner.getLevelFor(*this);

    Vector3i target = position;
    while (target.y < level.getMaxY()) {
        const BlockState existing = level.getBlockState(target.x, target.y, target.z);
        if (BlockSupport::isReplaceable(existing))
            break;

        ++target.y;
    }

    const BlockState existing = level.getBlockState(position.x, position.y, position.z);
    const bool blocked = existing.mName != "minecraft:air" && FallingBlockSystem::isTransparent(existing) &&
                         !BlockSupport::isReplaceable(existing);

    if (blocked) {
        FallingBlockSystem::spawnDestroyParticle(owner, level, position, mBlockState);
        _dropItem(owner);
        return;
    }

    if (mBreakOnGround) {
        FallingBlockSystem::spawnDestroyParticle(owner, level, target, mBlockState);
        _dropItem(owner);
        return;
    }

    BlockState placed = mBlockState;

    if (FallingBlockSystem::isConcretePowder(placed.mName) &&
        FallingBlockSystem::isTouchingWater(level, target))
        placed = BlockState(FallingBlockSystem::getConcreteFor(placed.mName));

    if (!BlockSupport::isReplaceable(existing))
        FallingBlockSystem::spawnDestroyParticle(owner, level, position, existing);

    _place(owner, target, placed);

    if (placed.mName == "minecraft:anvil" || placed.mName == "minecraft:chipped_anvil" ||
        placed.mName == "minecraft:damaged_anvil") {
        _onAnvilLanded(owner, target, placed);
        return;
    }

    if (placed.mName == "minecraft:pointed_dripstone")
        _onDripstoneLanded(owner, target, placed);
}

void FallingBlockActor::_place(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state) {
    FallingBlockSystem::setBlockState(owner, owner.getLevelFor(*this), position, state);
}

void FallingBlockActor::_onAnvilLanded(ServerNetworkHandler &owner, const Vector3i &position,
                                       const BlockState &state) {
    _damageEntitiesAt(owner, position, ANVIL_DEATH_KEY);

    Level &level = owner.getLevelFor(*this);
    if (getFallDistance() > (float) ANVIL_BREAK_FALL_DISTANCE) {
        const std::string next = FallingBlockSystem::getNextAnvilDamage(state.mName);

        if (next.empty())
            FallingBlockSystem::setBlockState(owner, level, position, BlockState("minecraft:air"));
        else
            FallingBlockSystem::setBlockState(owner, level, position, BlockState(next));
    }

    const Vector3f soundPosition((float) position.x + 0.5f, (float) position.y + 0.5f,
                                 (float) position.z + 0.5f);
    owner.playNamedSound(level, ANVIL_LAND_SOUND, soundPosition, ANVIL_LAND_VOLUME, ANVIL_LAND_PITCH);
}

void FallingBlockActor::_onDripstoneLanded(ServerNetworkHandler &owner, const Vector3i &position,
                                           const BlockState &state) {
    (void) state;

    _damageEntitiesAt(owner, position, DRIPSTONE_DEATH_KEY);

    const Vector3f soundPosition((float) position.x + 0.5f, (float) position.y + 0.5f,
                                 (float) position.z + 0.5f);
    owner.playNamedSound(owner.getLevelFor(*this), DRIPSTONE_LAND_SOUND, soundPosition, ANVIL_LAND_VOLUME,
                         ANVIL_LAND_PITCH);
}

void FallingBlockActor::_damageEntitiesAt(ServerNetworkHandler &owner, const Vector3i &position,
                                          const std::string &deathKey) {
    const float fallDistance = getFallDistance();
    if (fallDistance <= 0.0f)
        return;

    const float damage = std::min(ANVIL_MAX_DAMAGE,
                                  std::max(0.0f, fallDistance * ANVIL_DAMAGE_PER_FALL_DISTANCE));
    if (damage <= 0.0f)
        return;

    const float minX = (float) position.x;
    const float maxX = (float) position.x + 1.0f;
    const float minY = (float) position.y;
    const float maxY = (float) position.y + 1.0f;
    const float minZ = (float) position.z;
    const float maxZ = (float) position.z + 1.0f;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.getDimension() != getDimension())
            continue;

        const ActorSize size = ActorClassRegistry::getSize("minecraft:player");
        const Vector3f playerPosition = player.getPosition();
        const float halfWidth = size.mWidth * 0.5f;

        if (playerPosition.x + halfWidth < minX || playerPosition.x - halfWidth > maxX)
            continue;
        if (playerPosition.y + size.mHeight < minY || playerPosition.y > maxY)
            continue;
        if (playerPosition.z + halfWidth < minZ || playerPosition.z - halfWidth > maxZ)
            continue;

        owner.applyDamage(player, damage, deathKey, {player.getName()}, true, false);
    }

    for (auto &entry: owner.getActors()) {
        ServerActor *actor = entry.second.get();
        if (actor == nullptr || actor == this || !actor->isAlive() || actor->isProjectile() ||
            actor->getDimension() != getDimension())
            continue;

        const ActorSize size = ActorClassRegistry::getSize(actor->getTypeId());
        const Vector3f actorPosition = actor->getPosition();
        const float halfWidth = size.mWidth * 0.5f;

        if (actorPosition.x + halfWidth < minX || actorPosition.x - halfWidth > maxX)
            continue;
        if (actorPosition.y + size.mHeight < minY || actorPosition.y > maxY)
            continue;
        if (actorPosition.z + halfWidth < minZ || actorPosition.z - halfWidth > maxZ)
            continue;

        owner.damageActor(*actor, damage, nullptr);
    }
}

Tag FallingBlockActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();

    data.put("Block", mBlockState.toNbt());
    data.putByte("BreakOnLava", mBreakOnLava ? 1 : 0);
    data.putByte("BreakOnGround", mBreakOnGround ? 1 : 0);
    data.putInt("AliveTicks", mAliveTicks);

    return data;
}

void FallingBlockActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);

    const Tag *block = data.get("Block");
    if (block != nullptr && block->isCompound()) {
        const std::string name = block->getString("name", mBlockState.mName);
        const Tag *states = block->get("states");

        mBlockState = states != nullptr && states->isCompound() ? BlockState(name, *states) : BlockState(name);
    }

    mBreakOnLava = data.getByte("BreakOnLava", mBreakOnLava ? 1 : 0) != 0;
    mBreakOnGround = data.getByte("BreakOnGround", mBreakOnGround ? 1 : 0) != 0;
    mAliveTicks = data.getInt("AliveTicks", mAliveTicks);
}
