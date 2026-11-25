#pragma once

#include "Core/Event/Event.h"
#include "Core/Event/EventSignal.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class Actor;
class ActorDamageSource;
class BlockState;
class FishingHookActor;
class Level;
class MobActor;
class ServerActor;
class ServerPlayer;
struct MobEffectInstance;
enum class HealCause;

// a hit about to land, before shields, armor and effects; the handler may change the damage and the push
struct ActorDamageBeforeEvent : public CancellableEvent {
    Actor &mActor;
    const ActorDamageSource &mSource;
    float mDamage;
    // the push an attacker gives: only a source with an attacker pushes
    float &mKnockbackHorizontal;
    float &mKnockbackVertical;

    ActorDamageBeforeEvent(Actor &actor, const ActorDamageSource &source, float damage, float &knockbackHorizontal,
                           float &knockbackVertical)
            : mActor(actor), mSource(source), mDamage(damage), mKnockbackHorizontal(knockbackHorizontal),
              mKnockbackVertical(knockbackVertical) {}
};

// an actor just flagged dead; the drops and experience are null unless they wait for this event to drop
struct ActorDeathBeforeEvent : public Event {
    ServerActor &mActor;
    ServerPlayer *mKiller;
    std::vector<ItemStack> *mDrops;
    int32_t *mExperience;

    ActorDeathBeforeEvent(ServerActor &actor, ServerPlayer *killer, std::vector<ItemStack> *drops, int32_t *experience)
            : mActor(actor), mKiller(killer), mDrops(drops), mExperience(experience) {}
};

// a player killed, before anything drops or the death message goes out
struct PlayerDeathBeforeEvent : public Event {
    ServerPlayer &mPlayer;
    std::string &mMessage;
    std::vector<ItemStack> &mDrops;
    int32_t &mExperience;
    bool &mKeepInventory;
    bool &mKeepExperience;

    PlayerDeathBeforeEvent(ServerPlayer &player, std::string &message, std::vector<ItemStack> &drops,
                           int32_t &experience, bool &keepInventory, bool &keepExperience)
            : mPlayer(player), mMessage(message), mDrops(drops), mExperience(experience), mKeepInventory(keepInventory),
              mKeepExperience(keepExperience) {}
};

// a dead player about to respawn; a changed spawn is an overworld position
struct PlayerRespawnBeforeEvent : public Event {
    ServerPlayer &mPlayer;
    Vector3f mSpawn;
    bool mSpawnChanged = false;

    PlayerRespawnBeforeEvent(ServerPlayer &player, const Vector3f &spawn) : mPlayer(player), mSpawn(spawn) {}
};

// a fishing rod reeling in; no hooked actor is a fish catch
struct PlayerFishBeforeEvent : public CancellableEvent {
    ServerPlayer &mAngler;
    FishingHookActor &mHook;
    Actor *mHooked;

    PlayerFishBeforeEvent(ServerPlayer &angler, FishingHookActor &hook, Actor *hooked)
            : mAngler(angler), mHook(hook), mHooked(hooked) {}
};

struct ActorMountBeforeEvent : public CancellableEvent {
    Actor &mRider;
    ServerActor &mVehicle;

    ActorMountBeforeEvent(Actor &rider, ServerActor &vehicle) : mRider(rider), mVehicle(vehicle) {}
};

// a rider about to leave its vehicle; the game finds the vehicle again after it: a handler may mount the rider
// elsewhere or remove the vehicle
struct ActorDismountBeforeEvent : public Event {
    Actor &mRider;
    Actor &mVehicle;

    ActorDismountBeforeEvent(Actor &rider, Actor &vehicle) : mRider(rider), mVehicle(vehicle) {}
};

struct ActorTameBeforeEvent : public CancellableEvent {
    MobActor &mActor;
    Actor &mTamer;

    ActorTameBeforeEvent(MobActor &actor, Actor &tamer) : mActor(actor), mTamer(tamer) {}
};

// a mob about to pick a new target; null when the target is not loaded
struct ActorTargetBeforeEvent : public CancellableEvent {
    MobActor &mActor;
    Actor *mTarget;

    ActorTargetBeforeEvent(MobActor &actor, Actor *target) : mActor(actor), mTarget(target) {}
};

// two mobs about to make a baby
struct ActorBreedBeforeEvent : public CancellableEvent {
    MobActor &mParent;
    MobActor &mPartner;
    std::string mBabyType;

    ActorBreedBeforeEvent(MobActor &parent, MobActor &partner, std::string babyType)
            : mParent(parent), mPartner(partner), mBabyType(std::move(babyType)) {}
};

// a falling block about to land as a block; the handler may change the block placed
struct ActorBlockChangeBeforeEvent : public CancellableEvent {
    ServerActor &mActor;
    Level &mLevel;
    Vector3i mBlockPosition;
    BlockState &mBlock;

    ActorBlockChangeBeforeEvent(ServerActor &actor, Level &level, const Vector3i &position, BlockState &block)
            : mActor(actor), mLevel(level), mBlockPosition(position), mBlock(block) {}
};

// experience points about to be added to a player
struct PlayerExperienceChangeBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    int32_t mXp;

    PlayerExperienceChangeBeforeEvent(ServerPlayer &player, int32_t xp) : mPlayer(player), mXp(xp) {}
};

// a player's food level about to change
struct PlayerFoodChangeBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    float mFood;
    float mPreviousFood;

    PlayerFoodChangeBeforeEvent(ServerPlayer &player, float food, float previousFood)
            : mPlayer(player), mFood(food), mPreviousFood(previousFood) {}
};

// health about to be given back to a living actor
struct ActorHealBeforeEvent : public CancellableEvent {
    Actor &mActor;
    float mHeal;
    HealCause mCause;

    ActorHealBeforeEvent(Actor &actor, float heal, HealCause cause) : mActor(actor), mHeal(heal), mCause(cause) {}
};

// an effect that would apply, about to be added or to replace the running one
struct ActorEffectAddBeforeEvent : public CancellableEvent {
    Actor &mActor;
    const MobEffectInstance &mEffect;

    ActorEffectAddBeforeEvent(Actor &actor, const MobEffectInstance &effect) : mActor(actor), mEffect(effect) {}
};

// a mob replaced by the actor it transformed into, before it drops its equipment
struct ActorTransformAfterEvent : public Event {
    MobActor &mActor;
    ServerActor &mResult;

    ActorTransformAfterEvent(MobActor &actor, ServerActor &result) : mActor(actor), mResult(result) {}
};

struct ActorBeforeSignals {
    EventSignal<ActorDamageBeforeEvent> mActorDamage;
    EventSignal<ActorDeathBeforeEvent> mActorDeath;
    EventSignal<PlayerDeathBeforeEvent> mPlayerDeath;
    EventSignal<PlayerRespawnBeforeEvent> mPlayerRespawn;
    EventSignal<PlayerFishBeforeEvent> mPlayerFish;
    EventSignal<ActorMountBeforeEvent> mActorMount;
    EventSignal<ActorDismountBeforeEvent> mActorDismount;
    EventSignal<ActorTameBeforeEvent> mActorTame;
    EventSignal<ActorTargetBeforeEvent> mActorTarget;
    EventSignal<ActorBreedBeforeEvent> mActorBreed;
    EventSignal<ActorBlockChangeBeforeEvent> mActorBlockChange;
    EventSignal<PlayerExperienceChangeBeforeEvent> mPlayerExperienceChange;
    EventSignal<PlayerFoodChangeBeforeEvent> mPlayerFoodChange;
    EventSignal<ActorHealBeforeEvent> mActorHeal;
    EventSignal<ActorEffectAddBeforeEvent> mActorEffectAdd;

    void clearActorBeforeSignals() {
        mActorDamage.clear();
        mActorDeath.clear();
        mPlayerDeath.clear();
        mPlayerRespawn.clear();
        mPlayerFish.clear();
        mActorMount.clear();
        mActorDismount.clear();
        mActorTame.clear();
        mActorTarget.clear();
        mActorBreed.clear();
        mActorBlockChange.clear();
        mPlayerExperienceChange.clear();
        mPlayerFoodChange.clear();
        mActorHeal.clear();
        mActorEffectAdd.clear();
    }
};

struct ActorAfterSignals {
    EventSignal<ActorTransformAfterEvent> mActorTransform;

    void clearActorAfterSignals() {
        mActorTransform.clear();
    }
};
