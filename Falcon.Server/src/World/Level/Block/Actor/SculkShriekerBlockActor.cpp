#include "World/Level/Block/Actor/SculkShriekerBlockActor.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Mob/Hostile/WardenActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorClassRegistry.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/SculkShriekerBlock.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Particle/ShriekParticle.h"
#include "World/Level/Vibration/GameEvent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

FALCON_REGISTER_BLOCK_ACTOR(SculkShriekerBlockActor);

namespace {
    const char *const TAG_LISTENER = "VibrationListener";
    const char *const TAG_WARNING_LEVEL = "QueuedResponseThreatLevel";
    const char *const SOUND_SHRIEK = "block.sculk_shrieker.shriek";
    const char *const SOUND_SUMMONED = "agitated";
    // levels 1 to 4: the deep answers closer each time, angry when no warden could come up
    const std::array<const char *, 4> REPLY_SOUNDS = {"nearby_close", "nearby_closer", "nearby_closest",
                                                      "listening_angry"};
    const int32_t REPLY_SPREAD = 10;
    const char *const SPAWN_EMERGING_EVENT = "minecraft:spawn_emerging";
    const int32_t SUMMON_TRIES = 20;
    const int32_t SUMMON_HORIZONTAL = 5;
    const int32_t SUMMON_VERTICAL = 6;
    const int32_t MAX_RIDE_DEPTH = 16;

    std::mt19937 &shriekerRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t nextInt(int32_t bound) {
        return std::uniform_int_distribution<int32_t>(0, bound - 1)(shriekerRandom());
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    float squaredDistance(const Vector3f &a, const Vector3f &b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    bool stateFlag(const BlockState &state, const char *name) {
        return state.mStates.getByte(name, 0) != 0;
    }

    BlockState withFlag(const BlockState &state, const char *name, bool on) {
        Tag states = state.mStates;
        states.putByte(name, on ? 1 : 0);
        return BlockState(state.mName, states);
    }

    // the shriek is silent under water (material 5 of the extra block)
    bool isWaterlogged(Level &level, const Vector3i &position) {
        const BlockState extra = level.getBlockStateAtLayer(position.x, position.y, position.z, 1);
        BlockMaterialType material;
        return BlockMaterial::find(extra, material) && material == BlockMaterialType::Water;
    }

    bool isLeaves(const BlockState &state) {
        return state.mName.find("leaves") != std::string::npos;
    }

    // room to stand: no collision here, and a block below that holds it up
    bool canStandAt(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState below = level.getBlockState(x, y - 1, z);
        return !BlockShape::hasCollision(level.getBlockState(x, y, z)) && BlockShape::hasCollision(below)
               && !isLeaves(below);
    }

    // the warden's whole box is free of blocks and liquids
    bool isUnobstructed(Level &level, const Vector3f &position, const ActorSize &size) {
        const float half = size.mWidth * 0.5f;
        for (int32_t x = (int32_t) std::floor(position.x - half); x <= (int32_t) std::floor(position.x + half); ++x) {
            const int32_t top = (int32_t) std::ceil(position.y + size.mHeight);
            for (int32_t y = (int32_t) std::floor(position.y); y < top; ++y) {
                for (int32_t z = (int32_t) std::floor(position.z - half); z <= (int32_t) std::floor(position.z + half);
                     ++z) {
                    const LiquidInfo liquid = level.getLiquidInfo(x, y, z);
                    if (BlockShape::hasCollision(level.getBlockState(x, y, z)) || liquid.water || liquid.lava)
                        return false;
                }
            }
        }
        return true;
    }
}

VibrationListener &SculkShriekerBlockActor::listener() {
    if (!mListener.has_value())
        mListener.emplace(*this, mPosition, RANGE);
    return *mListener;
}

Tag SculkShriekerBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    if (mWarningLevel.has_value())
        data.putInt(TAG_WARNING_LEVEL, *mWarningLevel);
    if (mListener.has_value()) {
        data.put(TAG_LISTENER, mListener->save());
    } else {
        Tag idle = Tag::ofCompound();
        idle.put("selector", Tag::ofCompound());
        data.put(TAG_LISTENER, idle);
    }
    return data;
}

void SculkShriekerBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;
    if (data.contains(TAG_WARNING_LEVEL))
        mWarningLevel = data.getInt(TAG_WARNING_LEVEL, 0);
    const Tag *saved = data.get(TAG_LISTENER);
    if (saved != nullptr && saved->isCompound())
        listener().load(*saved);
}

bool SculkShriekerBlockActor::tick(ServerNetworkHandler &owner) {
    (void) owner;
    if (mLevel != nullptr)
        listener().tick(*mLevel);
    return true;
}

// only a sculk sensor's click reaches it
bool SculkShriekerBlockActor::canReceive(const GameEvent &event) const {
    return &event == &GameEvents::SCULK_SENSOR_TENDRILS_CLICKING;
}

bool SculkShriekerBlockActor::shouldListen(Level &level, const GameEvent &event, const GameEventContext &context) {
    (void) event;
    (void) context;
    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    return state != nullptr && state->mName == BlockNames::SCULK_SHRIEKER
           && !stateFlag(*state, SculkShriekerBlock::STATE_ACTIVE);
}

void SculkShriekerBlockActor::onSignalReceive(Level &level, const Vector3i &position, const GameEvent &event,
                                              Actor *source, float distance, uint32_t range,
                                              Actor *projectileOwner) {
    (void) position;
    (void) event;
    (void) distance;
    (void) range;
    ServerNetworkHandler *owner = level.getOwner();
    if (owner == nullptr)
        return;
    if (ServerPlayer *player = responsiblePlayer(*owner, projectileOwner != nullptr ? projectileOwner : source))
        tryShriek(*owner, level, *player);
}

ServerPlayer *SculkShriekerBlockActor::responsiblePlayer(ServerNetworkHandler &owner, Actor *actor) {
    for (int32_t depth = 0; actor != nullptr && depth < MAX_RIDE_DEPTH; ++depth) {
        if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(actor))
            return player;
        // a vehicle answers for the one in its controlling seat
        if (!actor->getPassengers().empty()) {
            actor = owner.getEntity(actor->getPassengers().front());
            continue;
        }
        const ServerActor *server = dynamic_cast<const ServerActor *>(actor);
        if (server == nullptr || !server->isProjectile() || server->getOwnerUniqueId() == -1)
            return nullptr;
        actor = owner.getEntity(server->getOwnerUniqueId());
    }
    return nullptr;
}

bool SculkShriekerBlockActor::_canRespond(ServerNetworkHandler &owner, Level &level) const {
    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    return state != nullptr && stateFlag(*state, SculkShriekerBlock::STATE_CAN_SUMMON)
           && owner.getProperties().getDifficulty() != Difficulty::Peaceful;
}

bool SculkShriekerBlockActor::tryShriek(ServerNetworkHandler &owner, Level &level, ServerPlayer &player) {
    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr || stateFlag(*state, SculkShriekerBlock::STATE_ACTIVE))
        return false;
    mWarningLevel.reset();
    if (_canRespond(owner, level) && !_tryWarn(owner, level, player))
        return false;
    _shriek(owner, level, player);
    return true;
}

// one warning shared by every player near: the most warned one's level, raised, unless any of them is on cooldown
bool SculkShriekerBlockActor::_tryWarn(ServerNetworkHandler &owner, Level &level, ServerPlayer &player) {
    const Vector3f center = centerOf(mPosition);
    const AxisAlignedBB area((float) mPosition.x + 0.5f - NEARBY_WARDEN_RANGE,
                             (float) mPosition.y + 0.5f - NEARBY_WARDEN_RANGE,
                             (float) mPosition.z + 0.5f - NEARBY_WARDEN_RANGE,
                             (float) mPosition.x + 0.5f + NEARBY_WARDEN_RANGE,
                             (float) mPosition.y + 0.5f + NEARBY_WARDEN_RANGE,
                             (float) mPosition.z + 0.5f + NEARBY_WARDEN_RANGE);
    bool wardenNear = false;
    owner.forEachActorNear(center, (float) NEARBY_WARDEN_RANGE * 2.0f, [&](ServerActor &actor) {
        if (!wardenNear && dynamic_cast<WardenActor *>(&actor) != nullptr && &owner.getLevelFor(actor) == &level
            && actor.isAlive() && area.intersectsWith(ActorPushSystem::boundingBoxOf(actor)))
            wardenNear = true;
    });
    if (wardenNear)
        return false;

    std::vector<ServerPlayer *> players;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &candidate = entry.second;
        if (&candidate == &player || !candidate.isSpawned() || candidate.isSpectator()
            || &owner.getLevelFor(candidate) != &level)
            continue;
        if (squaredDistance(candidate.getPosition(), center) < WARNING_RANGE * WARNING_RANGE)
            players.push_back(&candidate);
    }
    players.push_back(&player);

    ServerPlayer *picked = players.front();
    for (ServerPlayer *candidate: players) {
        if (candidate->getWardenSpawnTracker().mLevel > picked->getWardenSpawnTracker().mLevel)
            picked = candidate;
    }
    for (ServerPlayer *candidate: players) {
        if (candidate->getWardenSpawnTracker().onCooldown())
            return false;
    }

    WardenSpawnTracker &tracker = picked->getWardenSpawnTracker();
    tracker.increase();
    for (ServerPlayer *candidate: players)
        candidate->getWardenSpawnTracker() = tracker;
    mWarningLevel = tracker.mLevel;
    return true;
}

void SculkShriekerBlockActor::_shriek(ServerNetworkHandler &owner, Level &level, ServerPlayer &player) {
    const BlockState state = level.getBlockState(mPosition.x, mPosition.y, mPosition.z);
    level.setBlock(mPosition, withFlag(state, SculkShriekerBlock::STATE_ACTIVE, true), true);
    level.scheduleUpdate(mPosition, SHRIEK_TICKS);
    level.addParticle(ShriekParticle(mPosition));
    if (!isWaterlogged(level, mPosition))
        owner.playLevelSound(level, SOUND_SHRIEK, Vector3f((float) mPosition.x, (float) mPosition.y,
                                                           (float) mPosition.z));
    level.postGameEvent(GameEvents::SHRIEK, centerOf(mPosition), &player);
}

bool SculkShriekerBlockActor::_summonWarden(ServerNetworkHandler &owner, Level &level) {
    const ActorSize size = ActorClassRegistry::getSize(WardenActor::IDENTIFIER);
    for (int32_t attempt = 0; attempt < SUMMON_TRIES; ++attempt) {
        const int32_t x = mPosition.x + nextInt(2 * SUMMON_HORIZONTAL + 1) - SUMMON_HORIZONTAL;
        const int32_t z = mPosition.z + nextInt(2 * SUMMON_HORIZONTAL + 1) - SUMMON_HORIZONTAL;
        for (int32_t y = mPosition.y + SUMMON_VERTICAL; y >= mPosition.y - SUMMON_VERTICAL; --y) {
            if (!canStandAt(level, x, y, z))
                continue;
            const Vector3f position((float) x + 0.5f, (float) y, (float) z + 0.5f);
            if (!isUnobstructed(level, position, size))
                break;
            ServerActor *warden = owner.spawnActor(level, WardenActor::IDENTIFIER, position, [](ServerActor &actor) {
                if (MobActor *mob = dynamic_cast<MobActor *>(&actor))
                    mob->setSpawnEvent(SPAWN_EMERGING_EVENT);
            });
            if (warden == nullptr)
                break;
            owner.playLevelSound(level, SOUND_SUMMONED, warden->getPosition(), WardenActor::IDENTIFIER);
            return true;
        }
    }
    return false;
}

// SculkShriekerBlock tick: past the shriek, a warden at the fourth warning, a reply from the deep below it,
// and darkness for everyone around
void SculkShriekerBlockActor::respond(ServerNetworkHandler &owner, Level &level) {
    const std::optional<int32_t> warning = mWarningLevel;
    mWarningLevel.reset();
    if (!warning.has_value() || !_canRespond(owner, level))
        return;

    const bool summoned = *warning >= WardenSpawnTracker::MAX_LEVEL && _summonWarden(owner, level);
    if (!summoned && *warning >= 1 && *warning <= WardenSpawnTracker::MAX_LEVEL) {
        const Vector3f position((float) (mPosition.x + nextInt(2 * REPLY_SPREAD + 1) - REPLY_SPREAD),
                                (float) (mPosition.y + nextInt(2 * REPLY_SPREAD + 1) - REPLY_SPREAD),
                                (float) (mPosition.z + nextInt(2 * REPLY_SPREAD + 1) - REPLY_SPREAD));
        owner.playLevelSound(level, REPLY_SOUNDS[(size_t) (*warning - 1)], position, WardenActor::IDENTIFIER);
    }

    const Vector3f center = centerOf(mPosition);
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || &owner.getLevelFor(player) != &level
            || !(player.isSurvival() || player.isAdventure()))
            continue;
        if (squaredDistance(player.getPosition(), center) < DARKNESS_RANGE * DARKNESS_RANGE)
            owner.addEffect(player, MobEffectId::Darkness, 0, DARKNESS_TICKS);
    }
}
