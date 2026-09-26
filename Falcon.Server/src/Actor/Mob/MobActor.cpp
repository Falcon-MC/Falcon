#include "Actor/Mob/MobActor.h"

#include "Actor/AI/Goal/BehaviorGoals.h"
#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/DamageCause.h"
#include "Actor/Definition/EntityDefinitions.h"
#include "Actor/Definition/EntityEvents.h"
#include "Actor/Definition/EntityFilter.h"
#include "Actor/ServerPlayer.h"
#include "Block/Block.h"
#include "Block/BlockState.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"
#include "Loot/LootItems.h"
#include "Loot/LootTableRegistry.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginManager.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    const char *const SPAWNED_EVENT = "minecraft:entity_spawned";
    const char *const BORN_EVENT = "minecraft:entity_born";
    const char *const TAG_COMPONENT_GROUPS = "ComponentGroups";
    const char *const TAG_AGE = "AgeTicks";
    const char *const TAG_BREED_COOLDOWN = "BreedCooldown";
    const char *const TAG_TAMED_BY = "TamedBy";
    const char *const TAG_SITTING = "Sitting";
    const char *const TAG_HOME = "HomePos";
    const float DEFAULT_MOVEMENT_SPEED = 0.25f;
    const int32_t LOVE_TICKS = 600;
    const int32_t BREED_COOLDOWN_TICKS = 6000;
    const float DEFAULT_AGEABLE_SECONDS = 1200.0f;
    const float DEFAULT_FEED_GROWTH = 0.1f;
    const int32_t TICKS_PER_SECOND = 20;
    const char *const LOOT_TABLE_PREFIX = "loot_tables/";
    const int64_t OWNER_SYNC_INTERVAL = 20;
    const char *const TIMER_COMPONENT = "minecraft:timer";
    const char *const TRANSFORMATION_COMPONENT = "minecraft:transformation";
    const char *const DAMAGE_SENSOR_COMPONENT = "minecraft:damage_sensor";
    const char *const SPELL_EFFECTS_COMPONENT = "minecraft:spell_effects";
    const char *const INSTANT_DESPAWN_COMPONENT = "minecraft:instant_despawn";
    const char *const BODY_ROTATION_BLOCKED_COMPONENT = "minecraft:body_rotation_blocked";
    const char *const KNOCKBACK_RESISTANCE_COMPONENT = "minecraft:knockback_resistance";
    const char *const KNOCKBACK_RESISTANCE_ATTRIBUTE = "minecraft:knockback_resistance";
    const char *const LEGACY_ZOMBIE_PIGMAN = "minecraft:pig_zombie";
    const char *const ZOMBIE_PIGMAN = "minecraft:zombie_pigman";

    std::mt19937 &lifecycleRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float numberIn(const json::Value *component, const char *key, float fallback) {
        const json::Value *value = component == nullptr ? nullptr : component->get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    bool flagIn(const json::Value &component, const char *key) {
        const json::Value *value = component.get(key);
        return value != nullptr && value->boolean(false);
    }

    int32_t secondsToTicks(float seconds) {
        return std::max(1, (int32_t) std::lround(seconds * (float) TICKS_PER_SECOND));
    }

    float weightedChoice(const json::Value &choices) {
        int32_t total = 0;
        for (const std::unique_ptr<json::Value> &choice: choices.mArray)
            total += std::max(0, choice->get("weight") != nullptr ? choice->get("weight")->integer(1) : 1);

        if (total <= 0)
            return numberIn(choices.mArray.front().get(), "value", 0.0f);

        int32_t roll = std::uniform_int_distribution<int32_t>(0, total - 1)(lifecycleRandom());
        for (const std::unique_ptr<json::Value> &choice: choices.mArray) {
            roll -= std::max(0, choice->get("weight") != nullptr ? choice->get("weight")->integer(1) : 1);
            if (roll < 0)
                return numberIn(choice.get(), "value", 0.0f);
        }
        return numberIn(choices.mArray.back().get(), "value", 0.0f);
    }

    int32_t timerTicks(const json::Value &timer) {
        const json::Value *choices = timer.get("random_time_choices");
        if (choices != nullptr && choices->isArray() && !choices->mArray.empty())
            return secondsToTicks(weightedChoice(*choices));

        const json::Value *time = timer.get("time");
        if (time == nullptr || !time->isArray())
            return secondsToTicks(time == nullptr ? 0.0f : (float) time->number(0.0));

        if (time->mArray.empty())
            return secondsToTicks(0.0f);

        const float minimum = (float) time->mArray.front()->number(0.0);
        const float maximum = (float) time->mArray.back()->number(0.0);
        const json::Value *randomInterval = timer.get("randomInterval");
        if ((randomInterval != nullptr && !randomInterval->boolean(true)) || maximum <= minimum)
            return secondsToTicks(minimum);

        return secondsToTicks(std::uniform_real_distribution<float>(minimum, maximum)(lifecycleRandom()));
    }

    int32_t transformationTicks(const json::Value &transformation) {
        const json::Value *delay = transformation.get("delay");
        if (delay == nullptr)
            return 0;
        if (!delay->isObject())
            return (int32_t) std::lround((float) delay->number(0.0) * (float) TICKS_PER_SECOND);

        float seconds = numberIn(delay, "value", 0.0f);
        const float rangeMin = numberIn(delay, "range_min", 0.0f);
        const float rangeMax = numberIn(delay, "range_max", 0.0f);
        if (rangeMax > rangeMin)
            seconds += std::uniform_real_distribution<float>(rangeMin, rangeMax)(lifecycleRandom());
        else
            seconds += rangeMin;

        return (int32_t) std::lround(seconds * (float) TICKS_PER_SECOND);
    }

    bool isAssistBlock(const json::Value &types, const std::string &name) {
        for (const std::unique_ptr<json::Value> &type: types.mArray) {
            const std::string identifier = type->string();
            if (identifier == name || "minecraft:" + identifier == name)
                return true;
        }
        return false;
    }

    std::mt19937 &experienceRandom() {
        static std::mt19937 generator(0x9E3779B9u);
        return generator;
    }

    std::mt19937 &lootRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

MobActor::MobActor(uint64_t runtimeId, const std::string &identifier) : ServerActor(runtimeId, identifier) {
}

void MobActor::tick(ServerNetworkHandler &owner) {
    if (!mDefinitionStarted) {
        mDefinitionStarted = true;
        if (getDefinition() != nullptr) {
            fireEvent(owner, !mSpawnEvent.empty() ? mSpawnEvent : mBorn ? BORN_EVENT : SPAWNED_EVENT);
            if (!mEquipmentInherited)
                mEquipment.equipFromTable(owner, *this);
        }
    }

    _tickTransformation(owner);
    if (mTransformed)
        return;

    if (_tickInstantDespawn(owner))
        return;

    if (!mGoalsRegistered || mGoalsDirty)
        _registerGoals(owner);

    if (mBodyDirty)
        _syncBody(owner);

    _tickLifecycle(owner);
    _tickSensors(owner);
    _tickSpellEffects();
    _tickTimer(owner);

    mGoalSelector.tick(owner, *this);
    if (_tickInstantDespawn(owner))
        return;

    mNavigation.tick(owner, *this);
    tickControls(owner);
    ServerActor::tick(owner);
}

void MobActor::_registerGoals(ServerNetworkHandler &owner) {
    mGoalsDirty = false;
    const bool sameGroups = mGoalGroups.size() == mComponentGroups.size()
                            && std::is_permutation(mGoalGroups.begin(), mGoalGroups.end(), mComponentGroups.begin());
    if (mGoalsRegistered && sameGroups)
        return;

    mGoalGroups = mComponentGroups;
    mGoalSelector.clear(owner, *this);
    registerGoals(mGoalSelector);
    if (mGoalSelector.isEmpty())
        BehaviorGoals::build(*this, mGoalSelector);

    mGoalsRegistered = true;
    mGoalsDirty = false;
}

void MobActor::_setFlag(ServerNetworkHandler &owner, ActorFlag flag, bool value) {
    if (getFlags().get(flag) == value)
        return;

    getFlags().set(flag, value);
    owner.syncActorFlags(*this);
}

void MobActor::_tickLifecycle(ServerNetworkHandler &owner) {
    if (mBreedCooldown > 0)
        mBreedCooldown--;

    if (mInteractCooldown > 0)
        mInteractCooldown--;

    if (isTamed() && owner.getCurrentTick() % OWNER_SYNC_INTERVAL == 0)
        _syncOwner(owner);

    if (mLoveTicks > 0 && --mLoveTicks == 0)
        _setFlag(owner, ActorFlag::InLove, false);

    const json::Value *ageable = getComponent("minecraft:ageable");
    if (ageable == nullptr || getComponent("minecraft:is_baby") == nullptr)
        return;

    const float duration = numberIn(ageable, "duration", DEFAULT_AGEABLE_SECONDS) * TICKS_PER_SECOND;
    if (++mAgeTicks < (int32_t) duration)
        return;

    mAgeTicks = 0;
    EntityEvents::fireTrigger(owner, *this, ageable->get("grow_up"));
}

void MobActor::_fireComponentEvent(ServerNetworkHandler &owner, const char *component) {
    EntityEvents::fireTrigger(owner, *this, getComponent(component), getTarget(owner));
}

void MobActor::_tickSensors(ServerNetworkHandler &owner) {
    if (mTargetAcquired) {
        mTargetAcquired = false;
        _fireComponentEvent(owner, "minecraft:on_target_acquired");
    }

    if (mTargetEscaped) {
        mTargetEscaped = false;
        _fireComponentEvent(owner, "minecraft:on_target_escape");
    }

    const json::Value *sensor = getComponent("minecraft:environment_sensor");
    const json::Value *triggers = sensor == nullptr ? nullptr : sensor->get("triggers");
    if (triggers == nullptr)
        return;

    const auto run = [this, &owner](const json::Value &trigger) {
        const json::Value *filters = trigger.get("filters");
        if (filters == nullptr || EntityFilter::test(*filters, owner, *this))
            EntityEvents::fireTrigger(owner, *this, &trigger);
    };

    if (!triggers->isArray()) {
        run(*triggers);
        return;
    }

    for (const std::unique_ptr<json::Value> &trigger: triggers->mArray)
        run(*trigger);
}

bool MobActor::senseDamage(ServerNetworkHandler &owner, float &amount, const DamageSource &source) {
    const json::Value *sensor = getComponent(DAMAGE_SENSOR_COMPONENT);
    const json::Value *triggers = sensor == nullptr ? nullptr : sensor->get("triggers");
    if (triggers == nullptr)
        return true;

    std::vector<const json::Value *> entries;
    if (triggers->isArray()) {
        for (const std::unique_ptr<json::Value> &entry: triggers->mArray)
            entries.push_back(entry.get());
    } else {
        entries.push_back(triggers);
    }

    Actor *other = source.mAttacker != nullptr ? source.mAttacker : source.mDamager;
    bool dealsDamage = true;
    for (const json::Value *trigger: entries) {
        const json::Value *cause = trigger->get("cause");
        if (cause != nullptr && !DamageCause::matches(cause->string(), source.mDeathMessageKey))
            continue;

        const json::Value *onDamage = trigger->get("on_damage");
        const json::Value *filters = onDamage == nullptr ? nullptr : onDamage->get("filters");
        if (filters != nullptr && !EntityFilter::test(*filters, owner, *this, other))
            continue;

        amount = std::max(0.0f, amount * numberIn(trigger, "damage_multiplier", 1.0f)
                                + numberIn(trigger, "damage_modifier", 0.0f));

        const json::Value *deals = trigger->get("deals_damage");
        if (deals != nullptr && (deals->isString() ? deals->mString != "yes" : !deals->boolean(true)))
            dealsDamage = false;

        const json::Value *sound = trigger->get("on_damage_sound_event");
        if (sound != nullptr)
            playDefinitionSound(owner, sound->string());

        EntityEvents::fireTrigger(owner, *this, onDamage, other);
    }

    return dealsDamage;
}

void MobActor::_tickSpellEffects() {
    const json::Value *spellEffects = getComponent(SPELL_EFFECTS_COMPONENT);
    if (spellEffects == mSpellEffectsComponent)
        return;

    mSpellEffectsComponent = spellEffects;
    if (spellEffects == nullptr)
        return;

    std::vector<const json::Value *> removals;
    if (const json::Value *remove = spellEffects->get("remove_effects")) {
        if (remove->isArray()) {
            for (const std::unique_ptr<json::Value> &entry: remove->mArray)
                removals.push_back(entry.get());
        } else {
            removals.push_back(remove);
        }
    }

    for (const json::Value *removal: removals) {
        MobEffectId effect;
        if (parseDefinitionMobEffect(removal->string(), effect))
            removeEffect(effect);
    }

    const json::Value *additions = spellEffects->get("add_effects");
    if (additions == nullptr || !additions->isArray())
        return;

    for (const std::unique_ptr<json::Value> &addition: additions->mArray) {
        const json::Value *name = addition->get("effect");
        MobEffectInstance instance;
        if (name == nullptr || !parseDefinitionMobEffect(name->string(), instance.mId))
            continue;

        instance.mDuration = (int32_t) std::lround(numberIn(addition.get(), "duration", 0.0f) * TICKS_PER_SECOND);
        instance.mAmplifier = (int32_t) numberIn(addition.get(), "amplifier", 0.0f);
        const json::Value *ambient = addition->get("ambient");
        instance.mAmbient = ambient != nullptr && ambient->boolean(false);
        const json::Value *visible = addition->get("visible");
        instance.mParticles = visible == nullptr || visible->boolean(true);
        addEffect(instance);
    }
}

void MobActor::_tickTimer(ServerNetworkHandler &owner) {
    const json::Value *timer = getComponent(TIMER_COMPONENT);
    if (timer != mTimerComponent) {
        mTimerComponent = timer;
        mTimerTicks = timer == nullptr ? 0 : timerTicks(*timer);
    }

    if (timer == nullptr || mTimerTicks < 0 || --mTimerTicks > 0)
        return;

    const json::Value *looping = timer->get("looping");
    mTimerTicks = looping == nullptr || looping->boolean(true) ? timerTicks(*timer) : -1;

    EntityEvents::fireTrigger(owner, *this, timer->get("time_down_event"));
}

void MobActor::_tickTransformation(ServerNetworkHandler &owner) {
    const json::Value *transformation = getComponent(TRANSFORMATION_COMPONENT);
    if (transformation != mTransformationComponent) {
        mTransformationComponent = transformation;
        if (transformation == nullptr)
            return;

        mTransformationTicks = transformationTicks(*transformation);
        const json::Value *sound = transformation->get("begin_transform_sound");
        if (sound != nullptr)
            playDefinitionSound(owner, sound->string());
    }

    if (transformation == nullptr)
        return;

    if (mTransformationTicks > 0) {
        const json::Value *delay = transformation->get("delay");
        const int32_t assist = delay != nullptr && delay->isObject() ? _transformationAssist(owner, *delay) : 0;
        mTransformationTicks -= 1 + assist;
        return;
    }

    _transform(owner, *transformation);
}

bool MobActor::_tickInstantDespawn(ServerNetworkHandler &owner) {
    mEntitySpawner.tick(owner, *this);
    if (getComponent(INSTANT_DESPAWN_COMPONENT) == nullptr)
        return false;

    mDespawned = true;
    return true;
}

int32_t MobActor::_transformationAssist(ServerNetworkHandler &owner, const json::Value &delay) {
    const json::Value *types = delay.get("block_types");
    const float assistChance = numberIn(&delay, "block_assist_chance", 0.0f);
    if (types == nullptr || !types->isArray() || assistChance <= 0.0f
        || std::uniform_real_distribution<float>(0.0f, 1.0f)(lifecycleRandom()) >= assistChance)
        return 0;

    const int32_t radius = (int32_t) numberIn(&delay, "block_radius", 0.0f);
    const int32_t configuredMax = (int32_t) numberIn(&delay, "block_max", 0.0f);
    const int32_t maximum = configuredMax > 0 ? configuredMax : radius;
    const float blockChance = numberIn(&delay, "block_chance", 0.0f);

    Level &level = owner.getLevelFor(*this);
    const Vector3f position = getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);

    int32_t found = 0;
    int32_t assist = 0;
    for (int32_t x = originX - radius; x <= originX + radius && found < maximum; ++x) {
        for (int32_t y = originY - radius; y <= originY + radius && found < maximum; ++y) {
            for (int32_t z = originZ - radius; z <= originZ + radius && found < maximum; ++z) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr || !isAssistBlock(*types, state->mName))
                    continue;

                ++found;
                if (std::uniform_real_distribution<float>(0.0f, 1.0f)(lifecycleRandom()) < blockChance)
                    ++assist;
            }
        }
    }

    return assist;
}

void MobActor::_transform(ServerNetworkHandler &owner, const json::Value &transformation) {
    const json::Value *into = transformation.get("into");
    std::string identifier = into == nullptr ? std::string() : into->string();
    if (identifier.empty())
        return;

    std::string spawnEvent;
    const size_t eventStart = identifier.find('<');
    if (eventStart != std::string::npos) {
        const size_t eventEnd = identifier.find('>', eventStart);
        spawnEvent = identifier.substr(eventStart + 1, eventEnd == std::string::npos ? std::string::npos
                                                                                     : eventEnd - eventStart - 1);
        identifier.resize(eventStart);
    }

    if (identifier == LEGACY_ZOMBIE_PIGMAN)
        identifier = ZOMBIE_PIGMAN;

    Level &level = owner.getLevelFor(*this);
    const bool preserveEquipment = flagIn(transformation, "preserve_equipment");
    const auto configure = [this, &spawnEvent, preserveEquipment](ServerActor &actor) {
        actor.setRotation(getRotation());
        actor.setNameTag(getNameTag());
        actor.setPersistent(isPersistent());

        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        if (mob == nullptr)
            return;

        mob->mSpawnEvent = spawnEvent;
        if (preserveEquipment) {
            mob->mEquipment = mEquipment;
            mob->mEquipmentInherited = true;
        }
    };

    ServerActor *result = owner.spawnActor(level, identifier, getPosition(), configure);
    if (result == nullptr) {
        mTransformationTicks = transformationTicks(transformation);
        return;
    }

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_ENTITY_TRANSFORM)) {
        PluginEvent transformEvent;
        transformEvent.mType = FALCON_EVENT_ENTITY_TRANSFORM;
        transformEvent.mEntity = this;
        transformEvent.mTarget = result;
        plugins->dispatch(transformEvent);
    }

    if (flagIn(transformation, "drop_equipment"))
        mEquipment.dropAll(owner, level, getPosition());

    const json::Value *sound = transformation.get("transformation_sound");
    if (sound != nullptr)
        playDefinitionSound(owner, sound->string());

    mTransformed = true;
}

void MobActor::playDefinitionSound(ServerNetworkHandler &owner, const std::string &sound) {
    if (sound.empty())
        return;

    Level &level = owner.getLevelFor(*this);
    if (sound.find('.') != std::string::npos)
        owner.playNamedSound(level, sound, getPosition(), 1.0f, 1.0f);
    else
        owner.playLevelSound(level, sound, getPosition(), getIdentifier());
}

bool MobActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    const ItemStack held = player.getInventory().getItemInHand();
    if (_tryInteract(owner, player) || _tryTame(owner, player, held) || _tryFeedBaby(owner, player, held)
        || _tryStartLove(owner, player, held) || _trySit(owner, player))
        return true;

    return ServerActor::onInteract(owner, player);
}

bool MobActor::_tryInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    const json::Value *interact = getComponent("minecraft:interact");
    if (interact == nullptr || mInteractCooldown > 0)
        return false;

    const json::Value *list = interact->get("interactions");
    std::vector<const json::Value *> interactions;
    if (list != nullptr && list->isArray()) {
        for (const std::unique_ptr<json::Value> &entry: list->mArray)
            interactions.push_back(entry.get());
    } else {
        interactions.push_back(list != nullptr ? list : interact);
    }

    for (const json::Value *interaction: interactions) {
        const json::Value *onInteract = interaction->get("on_interact");
        const json::Value *filters = onInteract == nullptr ? nullptr : onInteract->get("filters");
        if (filters != nullptr && !EntityFilter::test(*filters, owner, *this, &player))
            continue;

        Level &level = owner.getLevelFor(*this);
        const json::Value *spawnItems = interaction->get("spawn_items");
        const json::Value *table = spawnItems == nullptr ? nullptr : spawnItems->get("table");
        if (table != nullptr)
            _spawnLoot(owner, level, table->string());

        const int32_t hurtItem = (int32_t) numberIn(interaction, "hurt_item", 0.0f);
        if (hurtItem > 0)
            owner.damagePlayerHeldItem(player, hurtItem);

        const json::Value *useItem = interaction->get("use_item");
        if (useItem != nullptr && useItem->boolean(false))
            player.consumeOneHeldItem();

        const json::Value *sound = interaction->get("play_sounds");
        if (sound != nullptr && sound->isString())
            owner.playLevelSound(level, sound->mString, getPosition(), getIdentifier());

        mInteractCooldown = (int32_t) (numberIn(interaction, "cooldown", 0.0f) * TICKS_PER_SECOND);

        EntityEvents::fireTrigger(owner, *this, onInteract, &player);
        return true;
    }
    return false;
}

void MobActor::_spawnLoot(ServerNetworkHandler &owner, Level &level, const std::string &path) {
    const std::string prefix = LOOT_TABLE_PREFIX;
    const std::string key = path.rfind(prefix, 0) == 0 ? path.substr(prefix.size()) : path;
    const LootTable *table = LootTableRegistry::getInstance().get(key);
    if (table == nullptr)
        return;

    LootContext context(lootRandom());
    context.mDifficulty = (int32_t) owner.getProperties().getDifficulty();
    const json::Value *color = getComponent("minecraft:color");
    context.mColorIndex = (int32_t) numberIn(color, "value", 0.0f);

    const Vector3f position = getPosition();
    for (const LootDrop &drop: table->roll(context)) {
        const ItemStack stack = LootItems::toItemStack(owner, drop);
        if (!stack.isAir())
            owner.dropItem(level, position, stack, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    }
}

bool MobActor::_tryTame(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held) {
    const json::Value *tameable = getComponent("minecraft:tameable");
    if (tameable == nullptr || isTamed() || !BehaviorItems(tameable->get("tame_items")).contains(held))
        return false;

    player.consumeOneHeldItem();
    const float probability = numberIn(tameable, "probability", 1.0f);
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(lifecycleRandom()) >= probability) {
        owner.broadcastActorEvent(*this, EntityEventType::TamingFailed);
        return true;
    }

    mTamedBy = player.getName();
    setPersistent(true);
    _setFlag(owner, ActorFlag::Tamed, true);
    _syncOwner(owner);
    owner.broadcastActorEvent(*this, EntityEventType::TamingSucceeded);

    EntityEvents::fireTrigger(owner, *this, tameable->get("tame_event"), &player);
    return true;
}

bool MobActor::_tryFeedBaby(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held) {
    const json::Value *ageable = getComponent("minecraft:ageable");
    if (ageable == nullptr || getComponent("minecraft:is_baby") == nullptr
        || !BehaviorItems(ageable->get("feed_items")).contains(held))
        return false;

    player.consumeOneHeldItem();
    const float duration = numberIn(ageable, "duration", DEFAULT_AGEABLE_SECONDS) * TICKS_PER_SECOND;
    mAgeTicks += (int32_t) (duration * DEFAULT_FEED_GROWTH);
    owner.broadcastActorEvent(*this, EntityEventType::BabyAge);
    return true;
}

bool MobActor::_tryStartLove(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held) {
    const json::Value *breedable = getComponent("minecraft:breedable");
    if (breedable == nullptr || getComponent("minecraft:is_baby") != nullptr || isInLove() || mBreedCooldown > 0
        || !BehaviorItems(breedable->get("breed_items")).contains(held))
        return false;

    const json::Value *requireTame = breedable->get("require_tame");
    if (requireTame != nullptr && requireTame->boolean(false) && !isTamed())
        return false;

    const json::Value *requireFullHealth = breedable->get("require_full_health");
    if (requireFullHealth != nullptr && requireFullHealth->boolean(false) && getHealth() < getMaxHealth())
        return false;

    player.consumeOneHeldItem();
    mLoveTicks = LOVE_TICKS;
    _setFlag(owner, ActorFlag::InLove, true);
    owner.broadcastActorEvent(*this, EntityEventType::InLoveHearts);
    return true;
}

bool MobActor::_trySit(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (getComponent("minecraft:sittable") == nullptr || !isTamed() || mTamedBy != player.getName())
        return false;

    mSitting = !mSitting;
    _setFlag(owner, ActorFlag::Sitting, mSitting);
    return true;
}

void MobActor::finishBreeding(ServerNetworkHandler &owner) {
    mLoveTicks = 0;
    mBreedCooldown = BREED_COOLDOWN_TICKS;
    _setFlag(owner, ActorFlag::InLove, false);
}

void MobActor::_syncBody(ServerNetworkHandler &owner) {
    mBodyDirty = false;
    const bool sameGroups = mBodyGroups.size() == mComponentGroups.size()
                            && std::is_permutation(mBodyGroups.begin(), mBodyGroups.end(), mComponentGroups.begin());
    if (getDefinition() == nullptr || (mBodySynced && sameGroups))
        return;

    mBodyGroups = mComponentGroups;
    mBodySynced = true;

    _setFlag(owner, ActorFlag::Baby, getComponent("minecraft:is_baby") != nullptr);
    _setFlag(owner, ActorFlag::Sheared, getComponent("minecraft:is_sheared") != nullptr);
    _setFlag(owner, ActorFlag::BodyRotationBlocked, getComponent(BODY_ROTATION_BLOCKED_COMPONENT) != nullptr);
    getAttributes().setClamped(KNOCKBACK_RESISTANCE_ATTRIBUTE,
                               numberIn(getComponent(KNOCKBACK_RESISTANCE_COMPONENT), "value", 0.0f));

    const float scale = numberIn(getComponent("minecraft:scale"), "value", 1.0f);
    if (scale != mScale) {
        mScale = scale;
        owner.syncActorScale(*this, scale);
    }

    EntityDataMap metadata;
    _appendDefinitionData(metadata);
    if (!metadata.mEntries.empty())
        owner.sendActorMetadata(*this, metadata);
}

void MobActor::_appendDefinitionData(EntityDataMap &metadata) const {
    const auto pushInt = [&metadata](int32_t id, int32_t value) {
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::Int;
        entry.mIntValue = value;
        metadata.mEntries.push_back(entry);
    };

    if (const json::Value *variant = getComponent("minecraft:variant"))
        pushInt(ActorFlags::VARIANT_DATA_ID, (int32_t) numberIn(variant, "value", 0.0f));
    if (const json::Value *mark = getComponent("minecraft:mark_variant"))
        pushInt(ActorFlags::MARK_VARIANT_DATA_ID, (int32_t) numberIn(mark, "value", 0.0f));

    if (const json::Value *color = getComponent("minecraft:color")) {
        EntityDataEntry entry;
        entry.mId = ActorFlags::COLOR_DATA_ID;
        entry.mFormat = EntityDataFormat::Byte;
        entry.mByteValue = (int8_t) numberIn(color, "value", 0.0f);
        metadata.mEntries.push_back(entry);
    }

    if (mOwnerRuntimeId != 0) {
        EntityDataEntry entry;
        entry.mId = ActorFlags::OWNER_DATA_ID;
        entry.mFormat = EntityDataFormat::Long;
        entry.mLongValue = (int64_t) mOwnerRuntimeId;
        metadata.mEntries.push_back(entry);
    }
}

void MobActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    const int32_t flagIds[] = {ActorFlags::FLAGS_DATA_ID, ActorFlags::FLAGS_2_DATA_ID};
    const int64_t flagValues[] = {getFlags().getLowBits(), getFlags().getHighBits()};
    for (int index = 0; index < 2; index++) {
        EntityDataEntry entry;
        entry.mId = flagIds[index];
        entry.mFormat = EntityDataFormat::Long;
        entry.mLongValue = flagValues[index];
        metadata.mEntries.push_back(entry);
    }

    EntityDataEntry scale;
    scale.mId = ActorFlags::SCALE_DATA_ID;
    scale.mFormat = EntityDataFormat::Float;
    scale.mFloatValue = mScale;
    metadata.mEntries.push_back(scale);

    _appendDefinitionData(metadata);
}

void MobActor::_syncOwner(ServerNetworkHandler &owner) {
    uint64_t runtimeId = 0;
    if (isTamed()) {
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.getName() == mTamedBy)
                runtimeId = entry.second.getRuntimeId();
        }
    }

    if (runtimeId == mOwnerRuntimeId)
        return;

    mOwnerRuntimeId = runtimeId;
    EntityDataMap metadata;
    EntityDataEntry entry;
    entry.mId = ActorFlags::OWNER_DATA_ID;
    entry.mFormat = EntityDataFormat::Long;
    entry.mLongValue = runtimeId == 0 ? -1 : (int64_t) runtimeId;
    metadata.mEntries.push_back(entry);
    owner.sendActorMetadata(*this, metadata);
}

const json::Value *MobActor::getDefinition() const {
    if (!mDefinitionResolved) {
        mDefinition = EntityDefinitions::find(getIdentifier());
        mDefinitionResolved = true;
    }
    return mDefinition;
}

void MobActor::_rebuildComponents() const {
    mComponents.clear();
    mComponentsDirty = false;

    const json::Value *definition = getDefinition();
    if (definition == nullptr)
        return;

    if (const json::Value *components = definition->get("components")) {
        for (const std::string &name: components->mKeys)
            mComponents[name] = components->mObject.at(name).get();
    }

    const json::Value *groups = definition->get("component_groups");
    if (groups == nullptr)
        return;

    for (const std::string &groupName: mComponentGroups) {
        const json::Value *group = groups->get(groupName);
        if (group == nullptr)
            continue;

        for (const std::string &name: group->mKeys)
            mComponents[name] = group->mObject.at(name).get();
    }
}

const std::unordered_map<std::string, const json::Value *> &MobActor::getComponents() const {
    if (mComponentsDirty)
        _rebuildComponents();
    return mComponents;
}

const json::Value *MobActor::getComponent(const std::string &name) const {
    const std::unordered_map<std::string, const json::Value *> &components = getComponents();
    const auto found = components.find(name);
    return found == components.end() ? nullptr : found->second;
}

std::vector<std::string> MobActor::getFamilies() const {
    std::vector<std::string> families;
    const json::Value *typeFamily = getComponent("minecraft:type_family");
    const json::Value *family = typeFamily == nullptr ? nullptr : typeFamily->get("family");
    if (family != nullptr) {
        for (const std::unique_ptr<json::Value> &entry: family->mArray)
            families.push_back(entry->string());
    }
    return families;
}

float MobActor::getMovementSpeed() const {
    const json::Value *movement = getComponent("minecraft:movement");
    const json::Value *value = movement == nullptr ? nullptr : movement->get("value");
    return value == nullptr ? DEFAULT_MOVEMENT_SPEED : (float) value->number(DEFAULT_MOVEMENT_SPEED);
}

bool MobActor::hasComponentGroup(const std::string &group) const {
    return std::find(mComponentGroups.begin(), mComponentGroups.end(), group) != mComponentGroups.end();
}

void MobActor::addComponentGroup(const std::string &group) {
    if (hasComponentGroup(group))
        return;

    mComponentGroups.push_back(group);
    _markComponentsChanged();
}

void MobActor::removeComponentGroup(const std::string &group) {
    const auto found = std::find(mComponentGroups.begin(), mComponentGroups.end(), group);
    if (found == mComponentGroups.end())
        return;

    mComponentGroups.erase(found);
    _markComponentsChanged();
}

void MobActor::_markComponentsChanged() {
    mComponentsDirty = true;
    mGoalsDirty = true;
    mBodyDirty = true;
}

void MobActor::markBorn() {
    mBorn = true;
}

bool MobActor::fireBlockEvent(ServerNetworkHandler &owner, const Vector3i &position, const std::string &event) {
    Level &level = owner.getLevelFor(*this);
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    if (state == nullptr)
        return false;

    const BlockState copy = *state;
    const Block *block = VanillaBlocks::fromIdentifier(copy.mName);
    return block != nullptr && block->onActorEvent(owner, level, position, copy, event, *this);
}

void MobActor::fireEvent(ServerNetworkHandler &owner, const std::string &event, Actor *other) {
    EntityEvents::fire(owner, *this, event, 0, other);
}

float MobActor::getAttackDamage(Difficulty difficulty) const {
    if (difficulty == Difficulty::Peaceful)
        return 0.0f;

    const json::Value *attack = getComponent("minecraft:attack");
    const json::Value *damage = attack == nullptr ? nullptr : attack->get("damage");
    if (damage == nullptr)
        return 0.0f;

    if (damage->isArray() && damage->mArray.size() == 2)
        return (float) randomRange(damage->mArray[0]->integer(0), damage->mArray[1]->integer(0));

    return (float) damage->number(0.0);
}

Tag MobActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();

    std::vector<Tag> groups;
    for (const std::string &group: mComponentGroups)
        groups.push_back(Tag::ofString(group));
    data.put(TAG_COMPONENT_GROUPS, Tag::ofList(Tag::Type::String, std::move(groups)));
    data.putInt(TAG_AGE, mAgeTicks);
    data.putInt(TAG_BREED_COOLDOWN, mBreedCooldown);
    data.putString(TAG_TAMED_BY, mTamedBy);
    data.putByte(TAG_SITTING, mSitting ? 1 : 0);
    if (mHasHome) {
        Tag home = Tag::ofList(Tag::Type::Float);
        home.addToList(Tag::ofFloat(mHomePosition.x));
        home.addToList(Tag::ofFloat(mHomePosition.y));
        home.addToList(Tag::ofFloat(mHomePosition.z));
        data.put(TAG_HOME, home);
    }
    mEquipment.saveNbt(data);
    mEntitySpawner.saveNbt(data);

    return data;
}

void MobActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);

    mAgeTicks = data.getInt(TAG_AGE, 0);
    mBreedCooldown = data.getInt(TAG_BREED_COOLDOWN, 0);
    mTamedBy = data.getString(TAG_TAMED_BY, std::string());
    mSitting = data.getByte(TAG_SITTING, 0) != 0;
    const Tag *home = data.get(TAG_HOME);
    if (home != nullptr && home->isList() && home->getList().size() == 3) {
        const std::vector<Tag> &values = home->getList();
        setHomePosition(Vector3f(values[0].asFloat(), values[1].asFloat(), values[2].asFloat()));
    }
    getFlags().set(ActorFlag::Tamed, !mTamedBy.empty());
    getFlags().set(ActorFlag::Sitting, mSitting);
    mEntitySpawner.loadNbt(data);

    const Tag *groups = data.get(TAG_COMPONENT_GROUPS);
    if (groups == nullptr || groups->getType() != Tag::Type::List)
        return;

    mComponentGroups.clear();
    for (const Tag &group: groups->getList())
        mComponentGroups.push_back(group.asString());

    mDefinitionStarted = true;
    _markComponentsChanged();
}

void MobActor::onDamaged(ServerNetworkHandler &owner, Actor *attacker) {
    mLastHurtTick = owner.getCurrentTick();
    mLastHurtBy = attacker != nullptr && attacker != this ? attacker->getRuntimeId() : 0;
    mHurtCount++;
}

bool MobActor::setTarget(ServerNetworkHandler &owner, uint64_t runtimeId) {
    if (runtimeId != 0 && runtimeId != mTargetRuntimeId) {
        if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_ENTITY_TARGET)) {
            PluginEvent targetEvent;
            targetEvent.mType = FALCON_EVENT_ENTITY_TARGET;
            targetEvent.mCancellable = true;
            targetEvent.mEntity = this;
            targetEvent.mTarget = findActor(owner, runtimeId);
            plugins->dispatch(targetEvent);
            if (targetEvent.mCancelled)
                return false;
        }
    }

    if (mTargetRuntimeId == 0 && runtimeId != 0)
        mTargetAcquired = true;
    mTargetRuntimeId = runtimeId;
    return true;
}

void MobActor::clearTarget() {
    if (mTargetRuntimeId != 0)
        mTargetEscaped = true;
    mTargetRuntimeId = 0;
}

Actor *MobActor::getTarget(ServerNetworkHandler &owner) const {
    if (mTargetRuntimeId == 0)
        return nullptr;

    Actor *target = findActor(owner, mTargetRuntimeId);
    return target != nullptr && canTarget(*target) ? target : nullptr;
}

bool MobActor::canTarget(const Actor &actor) const {
    if (&actor == this || actor.getDimension() != getDimension() || !actor.isAlive() || actor.isDead())
        return false;

    const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(&actor);
    if (player == nullptr)
        return true;

    const int32_t gameType = player->getGameType();
    return player->isSpawned() && gameType != (int32_t) GameType::Creative
           && gameType != (int32_t) GameType::Spectator;
}

Actor *MobActor::findActor(ServerNetworkHandler &owner, uint64_t runtimeId) {
    if (ServerPlayer *player = findPlayer(owner, runtimeId))
        return player;
    return owner.getActor((int64_t) runtimeId);
}

float MobActor::distanceSquaredTo(const Actor &other) const {
    const Vector3f position = getPosition();
    const Vector3f otherPosition = other.getPosition();
    const float dx = position.x - otherPosition.x;
    const float dy = position.y - otherPosition.y;
    const float dz = position.z - otherPosition.z;
    return dx * dx + dy * dy + dz * dz;
}

ServerPlayer *MobActor::findPlayer(ServerNetworkHandler &owner, uint64_t runtimeId) {
    for (auto &entry: owner.getPlayers()) {
        if (entry.second.getRuntimeId() == runtimeId)
            return &entry.second;
    }
    return nullptr;
}

void MobActor::tickControls(ServerNetworkHandler &owner) {
    if (!getFlags().get(ActorFlag::BodyRotationBlocked))
        mBodyControl.tick(*this, mMoveControl, mLookControl);
    mMoveControl.tick(owner, *this, mJumpControl);
    mJumpControl.tick(owner, *this);
    mLookControl.tick(*this);

    if (getFlags().get(ActorFlag::Moving) != mMoveControl.isMoving()) {
        getFlags().set(ActorFlag::Moving, mMoveControl.isMoving());
        owner.syncActorFlags(*this);
    }
}

float MobActor::resolveMaxHealth(Difficulty difficulty) const {
    (void) difficulty;
    return getDefaultMaxHealth();
}

void MobActor::applyDefaults(Difficulty difficulty) {
    const float health = resolveMaxHealth(difficulty);
    setMaxHealth(health);
    setHealth(health);
}

const LootTable *MobActor::getLootTable() const {
    return LootTableRegistry::getInstance().getForEntity(getTypeId());
}

int MobActor::randomRange(int minimum, int maximum) {
    if (minimum >= maximum)
        return minimum;

    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(experienceRandom());
}

void MobActor::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t lootingLevel) {
    if (isDead())
        return;

    if (owner.getLevel().getGameRules().getBool("domobloot")) {
        Level &level = owner.getLevelFor(*this);
        dropLoot(owner, level, source, lootingLevel);
        mEquipment.dropOnDeath(owner, level, *this, source != nullptr, lootingLevel);

        const int experience = getExperienceDrop();
        if (experience > 0 && source != nullptr)
            owner.spawnExperienceOrbs(level, getPosition(), experience);
    }

    ServerActor::kill(owner, source, lootingLevel);
}

void MobActor::dropLoot(ServerNetworkHandler &owner, Level &level, const ServerPlayer *killer,
                        int32_t lootingLevel) const {
    const LootTable *table = getLootTable();
    if (table == nullptr)
        return;

    LootContext context(lootRandom());
    context.mLootingLevel = lootingLevel;
    context.mDifficulty = (int32_t) owner.getProperties().getDifficulty();
    context.mRegionalDifficulty = level.getRegionalDifficulty(context.mDifficulty);
    context.mKilledByPlayer = killer != nullptr;
    context.mOnFire = isOnFire();
    if (killer != nullptr)
        context.mKillerIdentifier = killer->getIdentifier();

    const Vector3f position = getPosition();
    for (const LootDrop &drop: table->roll(context)) {
        const ItemStack stack = LootItems::toItemStack(owner, drop);
        if (!stack.isAir())
            owner.dropItem(level, position, stack, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    }
}
