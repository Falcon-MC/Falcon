#pragma once

#include "Core/Event/Event.h"
#include "Core/Event/EventSignal.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Actor/MobEffect.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class Actor;
class Level;
class NetworkIdentifier;
class Packet;
class ServerActor;
class ServerPlayer;
class Tag;
class World;

// a sound or particle packet about to reach the players in range; the handler may change who gets it
struct WorldEffectBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3f mPosition;
    const Packet &mPacket;
    std::vector<NetworkIdentifier> &mViewers;

    WorldEffectBeforeEvent(Level &level, const Vector3f &position, const Packet &packet,
                           std::vector<NetworkIdentifier> &viewers)
            : mLevel(level), mPosition(position), mPacket(packet), mViewers(viewers) {}
};

// an explosion about to hurt and break, its blocks already found
struct ExplosionBeforeEvent : public CancellableEvent {
    Level &mLevel;
    // null for an explosion no actor caused
    const ServerActor *mSource;
    Vector3f mCenter;
    double mSize;
    std::vector<Vector3i> &mBlocks;

    ExplosionBeforeEvent(Level &level, const ServerActor *source, const Vector3f &center, double size,
                         std::vector<Vector3i> &blocks)
            : mLevel(level), mSource(source), mCenter(center), mSize(size), mBlocks(blocks) {}
};

// an actor (tnt, creeper, crystal...) about to explode
struct ActorExplodeBeforeEvent : public CancellableEvent {
    Level &mLevel;
    const ServerActor &mSource;
    Vector3f mCenter;
    double mSize;
    bool mBreakBlocks = true;
    // 0 to 1
    double mFireChance = 0.0;

    ActorExplodeBeforeEvent(Level &level, const ServerActor &source, const Vector3f &center, double size)
            : mLevel(level), mSource(source), mCenter(center), mSize(size) {}
};

// an actor built and configured, not registered yet
struct ActorSpawnBeforeEvent : public CancellableEvent {
    Level &mLevel;
    ServerActor &mActor;

    ActorSpawnBeforeEvent(Level &level, ServerActor &actor) : mLevel(level), mActor(actor) {}
};

// a player's projectile spawned with its motion; cancelling removes it
struct PlayerShootBowBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    ServerActor &mProjectile;
    const ItemStack &mWeapon;
    // the launch speed
    float mForce;
    // an actor shot in place of the projectile, kept only when it shares the player's level
    std::optional<uint64_t> mReplacementRuntimeId;

    PlayerShootBowBeforeEvent(ServerPlayer &player, ServerActor &projectile, const ItemStack &weapon, float force)
            : mPlayer(player), mProjectile(projectile), mWeapon(weapon), mForce(force) {}
};

// a projectile on its way; cancelling removes it
struct ProjectileLaunchBeforeEvent : public CancellableEvent {
    ServerActor &mProjectile;
    // null for a dispensed one
    Actor *mShooter;

    ProjectileLaunchBeforeEvent(ServerActor &projectile, Actor *shooter) : mProjectile(projectile), mShooter(shooter) {}
};

// a projectile about to hit an actor or a block
struct ProjectileHitBeforeEvent : public CancellableEvent {
    ServerActor &mProjectile;
    Level &mLevel;
    Vector3f mPosition;
    Actor *mTarget;
    std::optional<Vector3i> mBlockPosition;

    ProjectileHitBeforeEvent(ServerActor &projectile, Level &level, const Vector3f &position, Actor *target,
                             std::optional<Vector3i> block)
            : mProjectile(projectile), mLevel(level), mPosition(position), mTarget(target), mBlockPosition(block) {}
};

// an actor about to burn; cancelling leaves it unlit
struct ActorCombustBeforeEvent : public CancellableEvent {
    Actor &mVictim;
    Actor *mSource;
    // set when a block lights it
    std::optional<Vector3i> mBlockPosition;
    // ticks
    int32_t mDuration;

    ActorCombustBeforeEvent(Actor &victim, Actor *source, std::optional<Vector3i> block, int32_t duration)
            : mVictim(victim), mSource(source), mBlockPosition(block), mDuration(duration) {}
};

// a lingering cloud about to apply its effects to the ones inside; cancelling spares them all
struct AreaEffectCloudApplyBeforeEvent : public CancellableEvent {
    ServerActor &mCloud;
    Level &mLevel;
    const std::vector<uint64_t> &mPlayerRuntimeIds;
    const std::vector<int64_t> &mActorUniqueIds;
    std::vector<MobEffectInstance> mEffects;
    // set when mEffects replaces the cloud's effects
    bool mEffectsChanged = false;
    int32_t mAge;
    int32_t mMaxAge;
    float mRadius;
    float mRadiusOnUse;
    float mRadiusPerTick;
    int32_t mReapplicationDelay;

    AreaEffectCloudApplyBeforeEvent(ServerActor &cloud, Level &level, const std::vector<uint64_t> &players,
                                    const std::vector<int64_t> &actors, std::vector<MobEffectInstance> effects,
                                    int32_t age, int32_t maxAge, float radius, float radiusOnUse, float radiusPerTick,
                                    int32_t reapplicationDelay)
            : mCloud(cloud), mLevel(level), mPlayerRuntimeIds(players), mActorUniqueIds(actors),
              mEffects(std::move(effects)), mAge(age), mMaxAge(maxAge), mRadius(radius), mRadiusOnUse(radiusOnUse),
              mRadiusPerTick(radiusPerTick), mReapplicationDelay(reapplicationDelay) {}
};

// a world created or opened
struct WorldOpenAfterEvent : public Event {
    World &mWorld;

    explicit WorldOpenAfterEvent(World &world) : mWorld(world) {}
};

// a world about to unload
struct WorldUnloadBeforeEvent : public CancellableEvent {
    World &mWorld;
    std::string mName;

    WorldUnloadBeforeEvent(World &world, const std::string &name) : mWorld(world), mName(name) {}
};

// a save starting, before anything is written
struct WorldSaveAfterEvent : public Event {
    World &mWorld;

    explicit WorldSaveAfterEvent(World &world) : mWorld(world) {}
};

// rain or thunder about to start or stop
struct WeatherChangeBeforeEvent : public CancellableEvent {
    Level &mLevel;
    // the weather it turns to
    bool mState;

    WeatherChangeBeforeEvent(Level &level, bool state) : mLevel(level), mState(state) {}
};

// a player about to move to another dimension or world
struct PlayerLevelChangeBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Level &mDestination;
    Vector3f mPosition;

    PlayerLevelChangeBeforeEvent(ServerPlayer &player, Level &destination, const Vector3f &position)
            : mPlayer(player), mDestination(destination), mPosition(position) {}
};

// a player's data about to be written; the handler may replace it
struct PlayerDataSaveBeforeEvent : public Event {
    const ServerPlayer &mPlayer;
    Tag &mData;

    PlayerDataSaveBeforeEvent(const ServerPlayer &player, Tag &data) : mPlayer(player), mData(data) {}
};

struct ChunkAfterEvent : public Event {
    Level &mLevel;
    int32_t mChunkX;
    int32_t mChunkZ;
    bool mGenerated;

    ChunkAfterEvent(Level &level, int32_t chunkX, int32_t chunkZ, bool generated)
            : mLevel(level), mChunkX(chunkX), mChunkZ(chunkZ), mGenerated(generated) {}
};

struct LevelBeforeSignals {
    EventSignal<WorldEffectBeforeEvent> mWorldSound;
    EventSignal<WorldEffectBeforeEvent> mWorldParticle;
    EventSignal<ExplosionBeforeEvent> mExplosion;
    EventSignal<ActorExplodeBeforeEvent> mActorExplode;
    EventSignal<ActorSpawnBeforeEvent> mActorSpawn;
    EventSignal<PlayerShootBowBeforeEvent> mPlayerShootBow;
    EventSignal<ProjectileLaunchBeforeEvent> mProjectileLaunch;
    EventSignal<ProjectileHitBeforeEvent> mProjectileHit;
    EventSignal<ActorCombustBeforeEvent> mActorCombustByBlock;
    EventSignal<ActorCombustBeforeEvent> mActorCombustByActor;
    EventSignal<AreaEffectCloudApplyBeforeEvent> mAreaEffectCloudApply;
    EventSignal<WorldUnloadBeforeEvent> mWorldUnload;
    EventSignal<WeatherChangeBeforeEvent> mRainChange;
    EventSignal<WeatherChangeBeforeEvent> mThunderChange;
    EventSignal<PlayerLevelChangeBeforeEvent> mPlayerChangeDimension;
    EventSignal<PlayerLevelChangeBeforeEvent> mPlayerChangeWorld;
    EventSignal<PlayerDataSaveBeforeEvent> mPlayerDataSave;

    void clearLevelBeforeSignals() {
        mWorldSound.clear();
        mWorldParticle.clear();
        mExplosion.clear();
        mActorExplode.clear();
        mActorSpawn.clear();
        mPlayerShootBow.clear();
        mProjectileLaunch.clear();
        mProjectileHit.clear();
        mActorCombustByBlock.clear();
        mActorCombustByActor.clear();
        mAreaEffectCloudApply.clear();
        mWorldUnload.clear();
        mRainChange.clear();
        mThunderChange.clear();
        mPlayerChangeDimension.clear();
        mPlayerChangeWorld.clear();
        mPlayerDataSave.clear();
    }
};

struct LevelAfterSignals {
    // the world is new: before its load
    EventSignal<WorldOpenAfterEvent> mWorldInit;
    EventSignal<WorldOpenAfterEvent> mWorldLoad;
    EventSignal<WorldSaveAfterEvent> mWorldSave;
    EventSignal<ChunkAfterEvent> mChunkLoad;
    EventSignal<ChunkAfterEvent> mChunkPopulate;
    EventSignal<ChunkAfterEvent> mChunkUnload;

    void clearLevelAfterSignals() {
        mWorldInit.clear();
        mWorldLoad.clear();
        mWorldSave.clear();
        mChunkLoad.clear();
        mChunkPopulate.clear();
        mChunkUnload.clear();
    }
};
