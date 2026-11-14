#include "World/Level/Block/Actor/MobSpawnerBlockActor.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/BlockEventPacket.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Level.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace {
    const char *const MOB_SPAWNER = "minecraft:mob_spawner";

    const char *const TAG_ENTITY_IDENTIFIER = "EntityIdentifier";
    const char *const TAG_DELAY = "Delay";
    const char *const TAG_MIN_SPAWN_DELAY = "MinSpawnDelay";
    const char *const TAG_MAX_SPAWN_DELAY = "MaxSpawnDelay";
    const char *const TAG_SPAWN_COUNT = "SpawnCount";
    const char *const TAG_MAX_NEARBY_ENTITIES = "MaxNearbyEntities";
    const char *const TAG_REQUIRED_PLAYER_RANGE = "RequiredPlayerRange";
    const char *const TAG_SPAWN_RANGE = "SpawnRange";
    const char *const TAG_DISPLAY_WIDTH = "DisplayEntityWidth";
    const char *const TAG_DISPLAY_HEIGHT = "DisplayEntityHeight";
    const char *const TAG_DISPLAY_SCALE = "DisplayEntityScale";
    const char *const TAG_SPAWN_DATA = "SpawnData";
    const char *const TAG_SPAWN_POTENTIALS = "SpawnPotentials";
    const char *const TAG_WEIGHT = "Weight";
    const char *const TAG_TYPE_ID = "TypeId";

    // the cap is counted in a box twice the spawn range wide and four blocks above and below the spawner
    const float CAP_BOX_HEIGHT = 4.0f;

    // the delay a spawner never runs down from: it takes a fresh one on its next turn
    const int32_t UNSET_DELAY = -1;

    // an identifier may carry the event its mob is spawned with
    std::string withoutEvent(const std::string &identifier) {
        const size_t event = identifier.find('<');
        return event == std::string::npos ? identifier : identifier.substr(0, event);
    }

    std::string eventOf(const std::string &identifier) {
        const size_t event = identifier.find('<');
        const size_t end = identifier.find('>', event);
        if (event == std::string::npos || end == std::string::npos)
            return std::string();
        return identifier.substr(event + 1, end - event - 1);
    }

    // the solid material flag on a block keeping its default full shape property
    bool blocksSpawn(const BlockState &state) {
        return (BlockMaterial::flagsOf(state) & BlockMaterial::FLAG_SOLID) != 0
               && BlockMaterial::hasDefaultProperty(state);
    }

    bool readPotential(const Tag &entry, MobSpawnerBlockActor::SpawnPotential &out) {
        if (!entry.isCompound())
            return false;
        out.mWeight = entry.getInt(TAG_WEIGHT, 0);
        out.mIdentifier = entry.getString(TAG_TYPE_ID, std::string());
        out.mData = entry;
        return true;
    }

    int16_t toShort(int32_t value) {
        return (int16_t) value;
    }
}

MobSpawnerBlockActor::MobSpawnerBlockActor() : mRandom(std::random_device{}()) {}

int32_t MobSpawnerBlockActor::_nextInt(int32_t bound) {
    return bound > 0 ? (int32_t) (mRandom() % (uint32_t) bound) : 0;
}

float MobSpawnerBlockActor::_nextFloat() {
    return (float) ((double) mRandom() * 2.3283064365386963e-10);
}

void MobSpawnerBlockActor::_writeFields(Tag &data) const {
    data.putString(TAG_ENTITY_IDENTIFIER, mEntityIdentifier);
    data.putShort(TAG_DELAY, toShort(mDelay));
    data.putShort(TAG_MIN_SPAWN_DELAY, toShort(mMinSpawnDelay));
    data.putShort(TAG_MAX_SPAWN_DELAY, toShort(mMaxSpawnDelay));
    data.putShort(TAG_SPAWN_COUNT, toShort(mSpawnCount));
    data.putShort(TAG_MAX_NEARBY_ENTITIES, toShort(mMaxNearbyEntities));
    data.putShort(TAG_REQUIRED_PLAYER_RANGE, toShort(mRequiredPlayerRange));
    data.putShort(TAG_SPAWN_RANGE, toShort(mSpawnRange));
    data.putFloat(TAG_DISPLAY_WIDTH, mDisplayWidth);
    data.putFloat(TAG_DISPLAY_HEIGHT, mDisplayHeight);
    data.putFloat(TAG_DISPLAY_SCALE, mDisplayScale);
    if (mSpawnData.has_value())
        data.put(TAG_SPAWN_DATA, mSpawnData->mData);
    if (!mSpawnPotentials.empty()) {
        Tag potentials = Tag::ofList(Tag::Type::Compound);
        for (const SpawnPotential &potential: mSpawnPotentials)
            potentials.addToList(potential.mData);
        data.put(TAG_SPAWN_POTENTIALS, potentials);
    }
}

Tag MobSpawnerBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _writeFields(data);
    return data;
}

Tag MobSpawnerBlockActor::getSpawnCompound() const {
    // the client spins the mob of the identifier and sizes it with the display fields
    Tag data = BlockActor::getSpawnCompound();
    _writeFields(data);
    return data;
}

void MobSpawnerBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    const std::string identifier = data.getString(TAG_ENTITY_IDENTIFIER, std::string());
    if (!identifier.empty())
        setEntityIdentifier(identifier);

    mDelay = data.getShort(TAG_DELAY, 0);

    mSpawnPotentials.clear();
    if (const Tag *potentials = data.get(TAG_SPAWN_POTENTIALS)) {
        if (potentials->isList()) {
            for (const Tag &entry: potentials->getList()) {
                SpawnPotential potential;
                if (readPotential(entry, potential))
                    mSpawnPotentials.push_back(std::move(potential));
            }
        }
    }

    mSpawnData.reset();
    if (const Tag *spawnData = data.get(TAG_SPAWN_DATA)) {
        SpawnPotential potential;
        if (readPotential(*spawnData, potential))
            mSpawnData = std::move(potential);
    }

    // each pair is only read when its first field is saved as a short
    if (data.contains(TAG_MIN_SPAWN_DELAY, Tag::Type::Short)) {
        mMinSpawnDelay = data.getShort(TAG_MIN_SPAWN_DELAY, 0);
        mMaxSpawnDelay = data.getShort(TAG_MAX_SPAWN_DELAY, 0);
        mSpawnCount = data.getShort(TAG_SPAWN_COUNT, 0);
    }
    if (data.contains(TAG_MAX_NEARBY_ENTITIES, Tag::Type::Short)) {
        mMaxNearbyEntities = data.getShort(TAG_MAX_NEARBY_ENTITIES, 0);
        mRequiredPlayerRange = data.getShort(TAG_REQUIRED_PLAYER_RANGE, 0);
    }
    if (data.contains(TAG_SPAWN_RANGE, Tag::Type::Short))
        mSpawnRange = data.getShort(TAG_SPAWN_RANGE, 0);

    if (data.contains(TAG_DISPLAY_WIDTH))
        mDisplayWidth = data.getFloat(TAG_DISPLAY_WIDTH, 0.0f);
    if (data.contains(TAG_DISPLAY_HEIGHT))
        mDisplayHeight = data.getFloat(TAG_DISPLAY_HEIGHT, 0.0f);
    if (data.contains(TAG_DISPLAY_SCALE))
        mDisplayScale = data.getFloat(TAG_DISPLAY_SCALE, 0.0f);
}

void MobSpawnerBlockActor::setEntityIdentifier(const std::string &identifier) {
    mEntityIdentifier = identifier;
    if (!mEntityIdentifier.empty())
        mHasEntity = true;
}

const std::string &MobSpawnerBlockActor::getSpawnIdentifier() const {
    return mSpawnData.has_value() ? mSpawnData->mIdentifier : mEntityIdentifier;
}

bool MobSpawnerBlockActor::isNearPlayer(ServerNetworkHandler &owner, Level &level) const {
    // no range: every player of the dimension counts
    const float range = mRequiredPlayerRange < 0 ? std::numeric_limits<float>::max()
                                                 : (float) mRequiredPlayerRange * (float) mRequiredPlayerRange;
    const Vector3f centre((float) mPosition.x + 0.5f, (float) mPosition.y + 0.5f, (float) mPosition.z + 0.5f);
    for (auto &entry: owner.getPlayers()) {
        const ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.isIn(level))
            continue;

        // the game measures from a player's eyes, where its position is kept
        const Vector3f &feet = player.getPosition();
        const float dx = feet.x - centre.x;
        const float dy = feet.y + MovementPhysics::PLAYER_BASE_OFFSET - centre.y;
        const float dz = feet.z - centre.z;
        if (dx * dx + dy * dy + dz * dz < range)
            return true;
    }
    return false;
}

int32_t MobSpawnerBlockActor::countNearby(ServerNetworkHandler &owner, Level &level,
                                         const std::string &identifier) const {
    const std::string kind = withoutEvent(identifier);
    const float reach = (float) mSpawnRange + (float) mSpawnRange;
    const float minX = (float) mPosition.x - reach;
    const float maxX = (float) mPosition.x + reach;
    const float minY = (float) mPosition.y - CAP_BOX_HEIGHT;
    const float maxY = (float) mPosition.y + CAP_BOX_HEIGHT;
    const float minZ = (float) mPosition.z - reach;
    const float maxZ = (float) mPosition.z + reach;

    int32_t count = 0;
    const Vector3f centre((float) mPosition.x, (float) mPosition.y, (float) mPosition.z);
    owner.forEachActorNear(centre, reach + CAP_BOX_HEIGHT, [&](ServerActor &actor) {
        if (!actor.isAlive() || !actor.isIn(level) || actor.getTypeId() != kind)
            return;

        const Vector3f &position = actor.getPosition();
        const ActorSize size = actor.getSize();
        const float half = size.mWidth * 0.5f;
        if (position.x - half <= maxX && minX <= position.x + half && position.y <= maxY
            && minY <= position.y + size.mHeight && position.z - half <= maxZ && minZ <= position.z + half)
            ++count;
    });
    return count;
}

void MobSpawnerBlockActor::_resetDelay(ServerNetworkHandler &owner, Level &level) {
    int32_t delay = mMinSpawnDelay;
    if (mMaxSpawnDelay > mMinSpawnDelay)
        delay += _nextInt(mMaxSpawnDelay - mMinSpawnDelay);
    mDelay = delay;

    if (!mSpawnPotentials.empty()) {
        int32_t total = 0;
        for (const SpawnPotential &potential: mSpawnPotentials)
            total += potential.mWeight;

        int32_t roll = total != 0 ? _nextInt(total) : 0;
        for (const SpawnPotential &potential: mSpawnPotentials) {
            roll -= potential.mWeight;
            if (roll < 0) {
                mSpawnData = potential;
                break;
            }
        }
    }

    BlockEventPacket event;
    event.mBlockPosition = mPosition;
    event.mEventType = SPAWN_EVENT;
    event.mEventData = 0;
    BlockActionHandler::broadcastToViewers(
            owner, level, Vector3f((float) mPosition.x + 0.5f, (float) mPosition.y + 0.5f, (float) mPosition.z + 0.5f),
            event);
}

bool MobSpawnerBlockActor::_trySpawn(ServerNetworkHandler &owner, Level &level, const std::string &identifier,
                                    bool &capped) {
    const float range = (float) mSpawnRange;
    const float x = (float) mPosition.x + (_nextFloat() - _nextFloat()) * range + 0.5f;
    const float y = (float) (mPosition.y + _nextInt(3) - 1);
    const float z = (float) mPosition.z + (_nextFloat() - _nextFloat()) * range + 0.5f;

    const int32_t blockX = (int32_t) std::floor(x);
    const int32_t blockY = (int32_t) std::floor(y);
    const int32_t blockZ = (int32_t) std::floor(z);
    const BlockState *state = level.peekBlockPtr(blockX, blockY, blockZ);
    if (state == nullptr || blocksSpawn(*state))
        return false;

    const int32_t offsetX = (int32_t) std::floor(x - (float) mPosition.x);
    const int32_t offsetY = (int32_t) std::floor(y - (float) mPosition.y);
    const int32_t offsetZ = (int32_t) std::floor(z - (float) mPosition.z);
    if (std::abs(offsetX) + std::abs(offsetY) + std::abs(offsetZ) > mSpawnRange)
        return false;

    if (countNearby(owner, level, identifier) >= mMaxNearbyEntities) {
        capped = true;
        return false;
    }

    const std::string event = eventOf(identifier);
    ServerActor *spawned = owner.spawnActor(level, withoutEvent(identifier), Vector3f(x, y, z),
                                            [&event](ServerActor &actor) {
                                                MobActor *mob = dynamic_cast<MobActor *>(&actor);
                                                if (mob != nullptr && !event.empty())
                                                    mob->setSpawnEvent(event);
                                            });
    if (spawned == nullptr)
        return false;

    const ActorSize size = spawned->getSize();
    const int32_t data = (int32_t) std::ceil(size.mWidth) + ((int32_t) std::ceil(size.mHeight) << 8);
    owner.broadcastLevelEvent(level, MOB_SPAWN_LEVEL_EVENT,
                              Vector3f((float) mPosition.x, (float) mPosition.y, (float) mPosition.z), data);
    return true;
}

bool MobSpawnerBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return true;

    Level &level = *mLevel;
    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;
    if (state->mName != MOB_SPAWNER)
        return false;

    if (!mHasEntity || !isNearPlayer(owner, level))
        return true;

    if (mDelay > 0) {
        --mDelay;
        return true;
    }

    if (mDelay == UNSET_DELAY) {
        _resetDelay(owner, level);
        return true;
    }

    const std::string identifier = getSpawnIdentifier();
    bool spawned = false;
    for (int32_t attempt = 0; attempt < mSpawnCount; ++attempt) {
        bool capped = false;
        spawned = _trySpawn(owner, level, identifier, capped) || spawned;
        if (capped) {
            _resetDelay(owner, level);
            return true;
        }
    }

    // a turn that spawned nothing tries again on the next tick
    if (spawned)
        _resetDelay(owner, level);
    return true;
}
