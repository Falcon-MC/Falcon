#include "Falcon/Plugin/PluginBusEvents.h"

#include "Core/Event/EventBus.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginWorldEventData.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Actor.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/FishingHookActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Level.h"

#include <string>

void PluginBusEvents::hookActor(PluginManager &plugins, EventBus &bus) {
    ActorBeforeSignals &before = bus.before();
    ActorAfterSignals &after = bus.after();

    plugins.hookBus(
            plugins.hasSubscribers(FALCON_EVENT_ENTITY_DAMAGE), before.mActorDamage,
            [&plugins](ActorDamageBeforeEvent &source) {
                const ActorDamageSource &damage = source.mSource;
                PluginEvent event;
                event.mType = FALCON_EVENT_ENTITY_DAMAGE;
                event.mCancellable = true;
                event.mCancelled = source.isCancelled();
                event.mEntity = &source.mActor;
                event.mAttacker = damage.mAttacker;
                event.mTarget = damage.mDamager;
                event.mAmount = source.mDamage;
                event.mCause = damage.mDeathMessageKey;
                if (damage.mBlock) {
                    event.mLevel = &plugins.getOwner().getLevelFor(source.mActor);
                    event.mBlockPosition = *damage.mBlock;
                    const Vector3i &position = *damage.mBlock;
                    if (const BlockState *block = event.mLevel->peekBlockPtr(position.x, position.y, position.z))
                        event.mBlockName = block->mName;
                }
                if (damage.mAttacker != nullptr) {
                    event.mKnockbackHorizontal = &source.mKnockbackHorizontal;
                    event.mKnockbackVertical = &source.mKnockbackVertical;
                }
                plugins.dispatch(event);
                source.setCancelled(event.mCancelled);
                source.mDamage = (float) event.mAmount;
            });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_DEATH), before.mActorDeath,
                    [&plugins](ActorDeathBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_DEATH;
                        event.mEntity = &source.mActor;
                        event.mAttacker = source.mKiller;
                        event.mDrops = source.mDrops;
                        event.mExperience = source.mExperience;
                        plugins.dispatch(event);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_DEATH), before.mPlayerDeath,
                    [&plugins](PlayerDeathBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_DEATH;
                        event.mPlayer = &source.mPlayer;
                        event.mEntity = &source.mPlayer;
                        event.mMessage = &source.mMessage;
                        event.mDrops = &source.mDrops;
                        event.mExperience = &source.mExperience;
                        event.mKeepInventory = &source.mKeepInventory;
                        event.mKeepExperience = &source.mKeepExperience;
                        plugins.dispatch(event);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_RESPAWN), before.mPlayerRespawn,
                    [&plugins](PlayerRespawnBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_RESPAWN;
                        event.mPlayer = &source.mPlayer;
                        event.mTo = source.mSpawn;
                        plugins.dispatch(event);
                        if (event.mToChanged) {
                            source.mSpawn = event.mTo;
                            source.mSpawnChanged = true;
                        }
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_FISH), before.mPlayerFish,
                    [&plugins](PlayerFishBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_FISH;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mAngler;
                        event.mEntity = &source.mHook;
                        event.mTarget = source.mHooked;
                        event.mState = source.mHooked == nullptr;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_MOUNT), before.mActorMount,
                    [&plugins](ActorMountBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_MOUNT;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mRider;
                        event.mTarget = &source.mVehicle;
                        event.mPlayer = dynamic_cast<ServerPlayer *>(&source.mRider);
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_DISMOUNT), before.mActorDismount,
                    [&plugins](ActorDismountBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_DISMOUNT;
                        event.mEntity = &source.mRider;
                        event.mTarget = &source.mVehicle;
                        event.mPlayer = dynamic_cast<ServerPlayer *>(&source.mRider);
                        plugins.dispatch(event);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_TAME), before.mActorTame,
                    [&plugins](ActorTameBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_TAME;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mActor;
                        event.mTarget = &source.mTamer;
                        event.mPlayer = dynamic_cast<ServerPlayer *>(&source.mTamer);
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_TARGET), before.mActorTarget,
                    [&plugins](ActorTargetBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_TARGET;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mActor;
                        event.mTarget = source.mTarget;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_BREED), before.mActorBreed,
                    [&plugins](ActorBreedBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_BREED;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mParent;
                        event.mTarget = &source.mPartner;
                        event.mCause = source.mBabyType;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_BLOCK_CHANGE), before.mActorBlockChange,
                    [&plugins](ActorBlockChangeBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_BLOCK_CHANGE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mActor;
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mBlockName = source.mBlock.mName;
                        const std::string original = PluginWorldEventData::encodeBlock(source.mBlock);
                        event.mCustomData = original;
                        plugins.dispatch(event);
                        // a block the plugin wrote that does not decode lands nothing
                        source.setCancelled(
                                event.mCancelled
                                || (event.mCustomData != original
                                    && !PluginWorldEventData::decodeBlock(event.mCustomData, source.mBlock)));
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_EXPERIENCE_CHANGE), before.mPlayerExperienceChange,
                    [&plugins](PlayerExperienceChangeBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_EXPERIENCE_CHANGE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mAmount = (double) source.mXp;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (!source.isCancelled())
                            source.mXp = (int32_t) event.mAmount;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_FOOD_CHANGE), before.mPlayerFoodChange,
                    [&plugins](PlayerFoodChangeBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_FOOD_CHANGE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mAmount = (double) source.mFood;
                        event.mPreviousAmount = (double) source.mPreviousFood;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (!source.isCancelled())
                            source.mFood = (float) event.mAmount;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_HEAL), before.mActorHeal,
                    [&plugins](ActorHealBeforeEvent &source) {
                        static const char *const CAUSES[] = {"heal", "regeneration", "self_heal", "totem"};
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_HEAL;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mActor;
                        event.mPlayer = dynamic_cast<ServerPlayer *>(&source.mActor);
                        event.mAmount = (double) source.mHeal;
                        event.mCause = CAUSES[(size_t) source.mCause];
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        source.mHeal = (float) event.mAmount;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_EFFECT_ADD), before.mActorEffectAdd,
                    [&plugins](ActorEffectAddBeforeEvent &source) {
                        const MobEffectInstance &instance = source.mEffect;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_EFFECT_ADD;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mActor;
                        event.mPlayer = dynamic_cast<ServerPlayer *>(&source.mActor);
                        event.mHasEffect = true;
                        event.mEffect.type = (FalconEffectType) instance.mId;
                        event.mEffect.duration = instance.mInfinite ? FALCON_EFFECT_INFINITE : instance.mDuration;
                        event.mEffect.amplifier = instance.mAmplifier;
                        event.mEffect.showParticles = instance.mParticles ? 1 : 0;
                        event.mEffect.ambient = instance.mAmbient ? 1 : 0;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_TRANSFORM), after.mActorTransform,
                    [&plugins](ActorTransformAfterEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_TRANSFORM;
                        event.mEntity = &source.mActor;
                        event.mTarget = &source.mResult;
                        plugins.dispatch(event);
                    });
}
