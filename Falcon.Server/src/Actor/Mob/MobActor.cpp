#include "Actor/Mob/MobActor.h"

#include "Actor/AI/Goal/BehaviorGoals.h"
#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/DamageCause.h"
#include "Actor/Definition/EntityDefinitions.h"
#include "Actor/Definition/EntityEvents.h"
#include "Actor/Definition/EntityFilter.h"
#include "Actor/Definition/Molang.h"
#include "Actor/RideSystem.h"
#include "Actor/ServerPlayer.h"
#include "Block/Block.h"
#include "Block/BlockState.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Item/Loot/LegacyItemMapper.h"
#include "Item/Loot/LootItems.h"
#include "Item/Loot/LootTableRegistry.h"
#include "Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginManager.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <random>

namespace {
    struct InteractParticle {
        const char *mType;
        const char *mEffect;
    };

    const InteractParticle INTERACT_PARTICLES[] = {
            {"smoke", "minecraft:basic_smoke_particle"},
            {"largeexplode", "minecraft:large_explosion"}
    };

    const char *const SPAWNED_EVENT = "minecraft:entity_spawned";
    const char *const BORN_EVENT = "minecraft:entity_born";
    const char *const TAG_COMPONENT_GROUPS = "ComponentGroups";
    const char *const TAG_DEFINITIONS = "definitions";
    const std::string DEFINITION_ADDED_PREFIX = "+";
    const char *const TAG_AGE = "AgeTicks";
    const char *const TAG_BREED_COOLDOWN = "BreedCooldown";
    const char *const TAG_TAMED_BY = "TamedBy";
    const char *const TAG_OWNER = "OwnerNew";
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
    const char *const ATTACK_COOLDOWN_COMPONENT = "minecraft:attack_cooldown";
    const char *const IS_SHAKING_COMPONENT = "minecraft:is_shaking";
    const char *const CELEBRATE_HUNT_COMPONENT = "minecraft:celebrate_hunt";
    const char *const INVENTORY_COMPONENT = "minecraft:inventory";
    const char *const EXPERIENCE_REWARD_COMPONENT = "minecraft:experience_reward";
    const char *const TRANSFORMATION_COMPONENT = "minecraft:transformation";
    const char *const DAMAGE_SENSOR_COMPONENT = "minecraft:damage_sensor";
    const char *const SPELL_EFFECTS_COMPONENT = "minecraft:spell_effects";
    const char *const INSTANT_DESPAWN_COMPONENT = "minecraft:instant_despawn";
    const char *const BODY_ROTATION_BLOCKED_COMPONENT = "minecraft:body_rotation_blocked";
    const char *const KNOCKBACK_RESISTANCE_COMPONENT = "minecraft:knockback_resistance";
    const char *const KNOCKBACK_RESISTANCE_ATTRIBUTE = "minecraft:knockback_resistance";
    const char *const LEGACY_ZOMBIE_PIGMAN = "minecraft:pig_zombie";
    const char *const ZOMBIE_PIGMAN = "minecraft:zombie_pigman";
    const char *const EQUIPPABLE_COMPONENT = "minecraft:equippable";
    const int32_t EQUIPPABLE_BODY_SLOT = 1;
    const char *const ENTITY_SENSOR_COMPONENT = "minecraft:entity_sensor";
    const float ENTITY_SENSOR_DEFAULT_RANGE = 10.0f;
    const char *const RIDEABLE_COMPONENT = "minecraft:rideable";
    const char *const TAMEMOUNT_COMPONENT = "minecraft:tamemount";
    const char *const JUMP_STRENGTH_COMPONENT = "minecraft:horse.jump_strength";
    const char *const DISMOUNT_ON_TOP_CENTER = "on_top_center";
    const char *const PLAYER_FAMILY = "player";
    const char *const MAD_SOUND = "mad";
    const int32_t DEFAULT_TEMPER_ATTEMPT_MOD = 5;
    const int32_t TAME_FAILED_REARING_TICKS = 20;
    const float DEFAULT_JUMP_STRENGTH = 0.7f;
    const char *const TAG_TEMPER = "Temper";
    const char *const TAG_MOVEMENT_SPEED = "RolledMovementSpeed";
    const char *const TAG_JUMP_STRENGTH = "RolledJumpStrength";

    std::mt19937 &lifecycleRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float numberIn(const json::Value *component, const char *key, float fallback) {
        const json::Value *value = component == nullptr ? nullptr : component->get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    float attributeIn(const Tag &data, const std::string &name, float fallback) {
        const Tag *attributes = data.get("Attributes");
        if (attributes == nullptr || !attributes->isList())
            return fallback;

        for (const Tag &attribute: attributes->getList()) {
            if (attribute.isCompound() && attribute.getString("Name", std::string()) == name)
                return attribute.getFloat("Base", fallback);
        }
        return fallback;
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
    _tickAttackCooldown(owner);
    _tickShaking(owner);
    _tickCelebration(owner);
    mAnger.tick(owner, *this);
    mAdmiration.tick(owner, *this);

    if (RideControlSystem::tick(owner, *this)) {
        ServerActor::tick(owner);
        RideSystem::syncPassengerPositions(owner, *this);
        return;
    }

    mGoalSelector.tick(owner, *this);
    if (_tickInstantDespawn(owner))
        return;

    mNavigation.tick(owner, *this);
    tickControls(owner);
    ServerActor::tick(owner);
    RideSystem::syncPassengerPositions(owner, *this);
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

    mLookedAtSensor.tick(owner, *this);
    _tickEntitySensor(owner);

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

void MobActor::_tickEntitySensor(ServerNetworkHandler &owner) {
    const json::Value *sensor = getComponent(ENTITY_SENSOR_COMPONENT);
    const json::Value *subsensors = sensor == nullptr ? nullptr : sensor->get("subsensors");
    if (subsensors == nullptr || !subsensors->isArray()) {
        mEntitySensorCooldowns.clear();
        return;
    }

    mEntitySensorCooldowns.resize(subsensors->mArray.size(), 0);
    const bool playersOnly = flagIn(*sensor, "find_players_only");
    const json::Value *relative = sensor->get("relative_range");
    const bool relativeRange = relative == nullptr || relative->boolean(true);

    for (size_t index = 0; index < subsensors->mArray.size(); ++index) {
        if (mEntitySensorCooldowns[index] > 0) {
            mEntitySensorCooldowns[index]--;
            continue;
        }

        const json::Value &subsensor = *subsensors->mArray[index];
        const int32_t count = _countSensedEntities(owner, subsensor, playersOnly, relativeRange);
        const int32_t minimum = (int32_t) numberIn(&subsensor, "minimum_count", 1.0f);
        const int32_t maximum = (int32_t) numberIn(&subsensor, "maximum_count", -1.0f);
        if (count < minimum || (maximum >= 0 && count > maximum))
            continue;

        const float cooldown = numberIn(&subsensor, "cooldown", -1.0f);
        if (cooldown > 0.0f)
            mEntitySensorCooldowns[index] = secondsToTicks(cooldown);

        const json::Value *event = subsensor.get("event");
        if (event != nullptr)
            fireEvent(owner, event->string());
    }
}

int32_t MobActor::_countSensedEntities(ServerNetworkHandler &owner, const json::Value &subsensor, bool playersOnly,
                                      bool relativeRange) {
    float horizontal = ENTITY_SENSOR_DEFAULT_RANGE;
    float vertical = ENTITY_SENSOR_DEFAULT_RANGE;
    const json::Value *range = subsensor.get("range");
    if (range != nullptr && range->isArray() && range->mArray.size() >= 2) {
        horizontal = (float) range->mArray[0]->number(horizontal);
        vertical = (float) range->mArray[1]->number(vertical);
    } else if (range != nullptr) {
        horizontal = (float) range->number(horizontal);
        vertical = horizontal;
    }

    if (relativeRange) {
        const ActorSize size = getSize();
        horizontal += size.mWidth * 0.5f;
        vertical += size.mHeight * 0.5f;
    }

    const Vector3f position = getPosition();
    const Vector3f center(position.x, position.y + numberIn(&subsensor, "y_offset", 0.0f), position.z);
    const json::Value *filters = subsensor.get("event_filters");

    const auto senses = [&](const Actor &actor) {
        if (&actor == this || !actor.isAlive() || actor.getDimension() != getDimension())
            return false;

        const Vector3f other = actor.getPosition();
        const float dx = other.x - center.x;
        const float dz = other.z - center.z;
        if (dx * dx + dz * dz > horizontal * horizontal || std::fabs(other.y - center.y) > vertical)
            return false;

        return filters == nullptr || EntityFilter::test(*filters, owner, *this, &actor);
    };

    int32_t count = 0;
    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned() && senses(entry.second))
            count++;
    }

    if (playersOnly)
        return count;

    for (auto &entry: owner.getActors()) {
        if (dynamic_cast<MobActor *>(entry.second.get()) != nullptr && senses(*entry.second))
            count++;
    }
    return count;
}

bool MobActor::senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) {
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
        break;
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

void MobActor::_tickAttackCooldown(ServerNetworkHandler &owner) {
    const json::Value *cooldown = getComponent(ATTACK_COOLDOWN_COMPONENT);
    if (cooldown != mAttackCooldownComponent) {
        mAttackCooldownComponent = cooldown;
        const json::Value *time = cooldown == nullptr ? nullptr : cooldown->get("attack_cooldown_time");
        float seconds = 0.0f;
        if (time != nullptr && time->isArray() && !time->mArray.empty()) {
            const float minimum = (float) time->mArray.front()->number(0.0);
            const float maximum = (float) time->mArray.back()->number(minimum);
            seconds = maximum > minimum ? std::uniform_real_distribution<float>(minimum, maximum)(lifecycleRandom())
                                        : minimum;
        } else if (time != nullptr) {
            seconds = (float) time->number(0.0);
        }
        mAttackCooldownTicks = cooldown == nullptr ? 0 : secondsToTicks(seconds);
    }

    if (cooldown == nullptr || mAttackCooldownTicks <= 0 || --mAttackCooldownTicks > 0)
        return;

    EntityEvents::fireTrigger(owner, *this, cooldown->get("attack_cooldown_complete_event"));
}

void MobActor::_tickShaking(ServerNetworkHandler &owner) {
    _setFlag(owner, ActorFlag::Shaking, getComponent(IS_SHAKING_COMPONENT) != nullptr);
}

void MobActor::onKilledActor(ServerNetworkHandler &owner, Actor &victim) {
    const json::Value *celebrate = getComponent(CELEBRATE_HUNT_COMPONENT);
    const json::Value *targets = celebrate == nullptr ? nullptr : celebrate->get("celebration_targets");
    const MobActor *victimMob = dynamic_cast<const MobActor *>(&victim);
    if (celebrate == nullptr
        || (targets != nullptr && (victimMob == nullptr || !EntityFilter::test(*targets, owner, *victimMob, this))))
        return;

    _startCelebration(owner, *celebrate);
    const json::Value *broadcast = celebrate->get("broadcast");
    if (broadcast == nullptr || !broadcast->boolean(false))
        return;

    const float radius = numberIn(celebrate, "radius", 0.0f);
    for (auto &entry: owner.getActors()) {
        MobActor *ally = dynamic_cast<MobActor *>(entry.second.get());
        if (ally == nullptr || ally == this || !ally->isAlive() || ally->getDimension() != getDimension()
            || distanceSquaredTo(*ally) > radius * radius || std::strcmp(ally->getIdentifier(), getIdentifier()) != 0)
            continue;

        const json::Value *allyCelebrate = ally->getComponent(CELEBRATE_HUNT_COMPONENT);
        if (allyCelebrate != nullptr)
            ally->_startCelebration(owner, *allyCelebrate);
    }
}

void MobActor::_startCelebration(ServerNetworkHandler &owner, const json::Value &component) {
    mCelebrationComponent = &component;
    mCelebrationTicks = secondsToTicks(numberIn(&component, "duration", 0.0f));
    mCelebrationSoundTicks = 0;
    _setFlag(owner, ActorFlag::Celebrating, true);
}

void MobActor::_tickCelebration(ServerNetworkHandler &owner) {
    if (mCelebrationTicks <= 0)
        return;

    if (getComponent(CELEBRATE_HUNT_COMPONENT) != mCelebrationComponent || --mCelebrationTicks <= 0) {
        mCelebrationTicks = 0;
        mCelebrationComponent = nullptr;
        _setFlag(owner, ActorFlag::Celebrating, false);
        return;
    }

    const json::Value *sound = mCelebrationComponent->get("celebrate_sound");
    if (sound == nullptr || --mCelebrationSoundTicks > 0)
        return;

    playDefinitionSound(owner, sound->string());
    const json::Value *interval = mCelebrationComponent->get("sound_interval");
    const float minimum = numberIn(interval, "range_min", 0.0f);
    const float maximum = numberIn(interval, "range_max", minimum);
    mCelebrationSoundTicks = secondsToTicks(maximum > minimum
                                            ? std::uniform_real_distribution<float>(minimum, maximum)(lifecycleRandom())
                                            : minimum);
}

int MobActor::getInventoryCapacity() const {
    return (int) numberIn(getComponent(INVENTORY_COMPONENT), "inventory_size", 0.0f);
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
    else if (flagIn(transformation, "drop_inventory"))
        mEquipment.dropInventory(owner, level, getPosition());

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
        || _tryStartLove(owner, player, held) || _trySit(owner, player) || _tryFeedMount(owner, player, held)
        || _tryMount(owner, player))
        return true;

    return ServerActor::onInteract(owner, player);
}

bool MobActor::_tryFeedMount(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &held) {
    const json::Value *tamemount = getComponent(TAMEMOUNT_COMPONENT);
    if (tamemount == nullptr || held.isAir())
        return false;

    const json::Value *rejected = tamemount->get("auto_reject_items");
    if (rejected != nullptr && rejected->isArray()) {
        for (const std::unique_ptr<json::Value> &entry: rejected->mArray) {
            const json::Value *item = entry->isObject() ? entry->get("item") : entry.get();
            if (item == nullptr || !BehaviorItems(item).contains(held))
                continue;

            owner.playLevelSound(owner.getLevelFor(*this), MAD_SOUND, getPosition(), getIdentifier());
            return true;
        }
    }

    const json::Value *feedItems = tamemount->get("feed_items");
    if (feedItems == nullptr || !feedItems->isArray())
        return false;

    for (const std::unique_ptr<json::Value> &entry: feedItems->mArray) {
        const json::Value *item = entry->isObject() ? entry->get("item") : entry.get();
        if (item == nullptr || !BehaviorItems(item).contains(held))
            continue;

        const int32_t maximum = (int32_t) numberIn(tamemount, "max_temper", 100.0f);
        mTemper = std::min(maximum, mTemper + (int32_t) numberIn(entry.get(), "temper_mod", 0.0f));
        player.consumeOneHeldItem();
        owner.playLevelSound(owner.getLevelFor(*this), "eat", getPosition(), getIdentifier());
        return true;
    }
    return false;
}

bool MobActor::_tryMount(ServerNetworkHandler &owner, ServerPlayer &player) {
    const json::Value *rideable = getComponent(RIDEABLE_COMPONENT);
    if (rideable == nullptr || player.isRiding() || !isAlive())
        return false;

    const json::Value *skip = rideable->get("crouching_skip_interact");
    if ((skip == nullptr || skip->boolean(true)) && player.getFlags().get(ActorFlag::Sneaking))
        return false;

    const json::Value *families = rideable->get("family_types");
    bool accepted = families == nullptr;
    if (families != nullptr && families->isArray()) {
        for (const std::unique_ptr<json::Value> &family: families->mArray) {
            if (family->string() == PLAYER_FAMILY)
                accepted = true;
        }
    }
    if (!accepted)
        return false;

    const size_t seats = (size_t) std::max(1, (int32_t) numberIn(rideable, "seat_count", 1.0f));
    if (getPassengers().size() >= seats)
        return false;

    return RideSystem::mount(owner, player, *this, true);
}

const json::Value *MobActor::_seatFor(size_t index, size_t passengerCount) const {
    const json::Value *rideable = getComponent(RIDEABLE_COMPONENT);
    const json::Value *seats = rideable == nullptr ? nullptr : rideable->get("seats");
    if (seats == nullptr)
        return nullptr;

    if (!seats->isArray())
        return seats;

    size_t applicable = 0;
    for (const std::unique_ptr<json::Value> &seat: seats->mArray) {
        const int32_t minimum = (int32_t) numberIn(seat.get(), "min_rider_count", 0.0f);
        const int32_t maximum = (int32_t) numberIn(seat.get(), "max_rider_count", (float) passengerCount);
        if ((int32_t) passengerCount < minimum || (int32_t) passengerCount > maximum)
            continue;

        if (applicable == index)
            return seat.get();
        applicable++;
    }
    return nullptr;
}

Vector3f MobActor::getSeatOffset(size_t index, size_t passengerCount) const {
    const json::Value *seat = _seatFor(index, passengerCount);
    const json::Value *position = seat == nullptr ? nullptr : seat->get("position");
    if (position == nullptr || !position->isArray() || position->mArray.size() < 3)
        return ServerActor::getSeatOffset(index, passengerCount);

    return Vector3f((float) position->mArray[0]->number(0.0) * mScale,
                    (float) position->mArray[1]->number(0.0) * mScale,
                    (float) position->mArray[2]->number(0.0) * mScale);
}

Vector3f MobActor::getDismountPosition(size_t index, size_t passengerCount) const {
    const json::Value *rideable = getComponent(RIDEABLE_COMPONENT);
    const json::Value *mode = rideable == nullptr ? nullptr : rideable->get("dismount_mode");
    if (mode == nullptr || mode->string() != DISMOUNT_ON_TOP_CENTER)
        return ServerActor::getDismountPosition(index, passengerCount);

    const Vector3f position = getPosition();
    return Vector3f(position.x, position.y + getSize().mHeight * mScale, position.z);
}

void MobActor::onPassengerAdded(ServerNetworkHandler &owner, Actor &passenger) {
    RideControlSystem::reset(*this);
    mNavigation.stop(*this);

    const json::Value *rideable = getComponent(RIDEABLE_COMPONENT);
    const json::Value *event = rideable == nullptr ? nullptr : rideable->get("on_rider_enter_event");
    if (event != nullptr)
        fireEvent(owner, event->string(), &passenger);
}

void MobActor::onPassengerRemoved(ServerNetworkHandler &owner, Actor &passenger) {
    RideControlSystem::reset(*this);
    _setFlag(owner, ActorFlag::Rearing, false);

    const json::Value *rideable = getComponent(RIDEABLE_COMPONENT);
    const json::Value *event = rideable == nullptr ? nullptr : rideable->get("on_rider_exit_event");
    if (event != nullptr)
        fireEvent(owner, event->string(), &passenger);
}

bool MobActor::isMountTaming() const {
    return getComponent(TAMEMOUNT_COMPONENT) != nullptr && !isTamed();
}

void MobActor::attemptMountTame(ServerNetworkHandler &owner, ServerPlayer &rider) {
    const json::Value *tamemount = getComponent(TAMEMOUNT_COMPONENT);
    if (tamemount == nullptr || isTamed())
        return;

    const int32_t maximum = (int32_t) numberIn(tamemount, "max_temper", 100.0f);
    if (maximum > 0 && std::uniform_int_distribution<int32_t>(0, maximum - 1)(lifecycleRandom()) < mTemper) {
        mOwnerId = rider.getUniqueId();
        mLegacyOwnerName.clear();
        setPersistent(true);
        _setFlag(owner, ActorFlag::Tamed, true);
        _syncOwner(owner);
        owner.broadcastActorEvent(*this, EntityEventType::TamingSucceeded);
        EntityEvents::fireTrigger(owner, *this, tamemount->get("tame_event"), &rider);
        return;
    }

    const int32_t modifier = (int32_t) numberIn(tamemount, "attempt_temper_mod", (float) DEFAULT_TEMPER_ATTEMPT_MOD);
    mTemper = std::min(maximum, mTemper + modifier);
    RideSystem::dismount(owner, rider, false);
    mRideControl.mRearingTicks = TAME_FAILED_REARING_TICKS;
    _setFlag(owner, ActorFlag::Rearing, true);
    owner.broadcastActorEvent(*this, EntityEventType::TamingFailed);
    owner.playLevelSound(owner.getLevelFor(*this), MAD_SOUND, getPosition(), getIdentifier());
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

        const int64_t hurtCooldown = (int64_t) (numberIn(interaction, "cooldown_after_being_attacked", 0.0f)
                                                * TICKS_PER_SECOND);
        if (hurtCooldown > 0 && owner.getCurrentTick() - mLastHurtTick < hurtCooldown)
            continue;

        const json::Value *admire = interaction->get("admire");
        const bool admires = admire != nullptr && admire->boolean(false);
        if (admires && !mAdmiration.canAdmire(owner, *this))
            continue;

        const json::Value *equipSlot = interaction->get("equip_item_slot");
        if (equipSlot != nullptr && !_equipFromHand(owner, player, equipSlot->string()))
            continue;

        if (admires) {
            const json::Value *barter = interaction->get("barter");
            mAdmiration.start(owner, *this, player.getInventory().getItemInHand(),
                              barter != nullptr && barter->boolean(false));
        }

        Level &level = owner.getLevelFor(*this);
        const json::Value *spawnItems = interaction->get("spawn_items");
        const json::Value *table = spawnItems == nullptr ? nullptr : spawnItems->get("table");
        if (table != nullptr)
            _spawnLoot(owner, level, table->string());

        const json::Value *dropSlot = interaction->get("drop_item_slot");
        if (dropSlot != nullptr)
            _dropEquipmentSlot(owner, dropSlot->string(), numberIn(interaction, "drop_item_y_offset", 0.0f));

        const int32_t hurtItem = (int32_t) numberIn(interaction, "hurt_item", 0.0f);
        if (hurtItem > 0)
            owner.damagePlayerHeldItem(player, hurtItem);

        const json::Value *useItem = interaction->get("use_item");
        if (equipSlot == nullptr && useItem != nullptr && useItem->boolean(false))
            player.consumeOneHeldItem();

        const json::Value *transform = interaction->get("transform_to_item");
        if (transform != nullptr && transform->isString()) {
            LootDrop drop;
            LegacyItemMapper::splitData(transform->mString, drop.mIdentifier, drop.mData);
            const ItemStack converted = LootItems::toItemStack(owner, drop);
            if (!converted.isAir())
                _givePlayerItem(owner, player, converted);
        }

        const json::Value *addItems = interaction->get("add_items");
        const json::Value *addTable = addItems == nullptr ? nullptr : addItems->get("table");
        if (addTable != nullptr) {
            for (ItemStack &stack: rollLoot(owner, addTable->string()))
                _givePlayerItem(owner, player, std::move(stack));
        }

        const json::Value *particle = interaction->get("particle_on_start");
        const json::Value *particleType = particle == nullptr ? nullptr : particle->get("particle_type");
        if (particleType != nullptr)
            _emitInteractParticle(owner, player, *particle, particleType->string());

        const json::Value *sound = interaction->get("play_sounds");
        if (sound != nullptr && sound->isString())
            owner.playLevelSound(level, sound->mString, getPosition(), getIdentifier());

        mInteractCooldown = (int32_t) (numberIn(interaction, "cooldown", 0.0f) * TICKS_PER_SECOND);

        EntityEvents::fireTrigger(owner, *this, onInteract, &player);
        return true;
    }
    return false;
}

const json::Value *MobActor::_equippableSlot(int32_t index) const {
    const json::Value *equippable = getComponent(EQUIPPABLE_COMPONENT);
    const json::Value *slots = equippable == nullptr ? nullptr : equippable->get("slots");
    if (slots == nullptr || !slots->isArray())
        return nullptr;

    for (const std::unique_ptr<json::Value> &slot: slots->mArray) {
        const json::Value *number = slot->get("slot");
        if (number != nullptr && number->integer(-1) == index)
            return slot.get();
    }
    return nullptr;
}

bool MobActor::_equipFromHand(ServerNetworkHandler &owner, ServerPlayer &player, const std::string &slotName) {
    ItemStack held = player.getInventory().getItemInHand();
    if (held.isAir() || held.mCount <= 0)
        return false;

    held.mCount = 1;
    const int32_t equipmentSlot = MobEquipment::slotIndexOf(slotName);
    if (equipmentSlot >= 0) {
        if (!mEquipment.getSlot(equipmentSlot).isAir())
            return false;

        mEquipment.setSlot(equipmentSlot, std::move(held));
        player.consumeOneHeldItem();
        mEquipment.broadcast(owner, *this);
        return true;
    }

    const int32_t inventorySlot = (int32_t) std::strtol(slotName.c_str(), nullptr, 10);
    const json::Value *slot = _equippableSlot(inventorySlot);
    if (slot == nullptr || !_equippableItem(inventorySlot).isAir())
        return false;

    const json::Value *accepted = slot->get("accepted_items");
    if (accepted != nullptr && !BehaviorItems(accepted).contains(held))
        return false;

    _setEquippableItem(inventorySlot, std::move(held));
    player.consumeOneHeldItem();
    mEquipment.broadcast(owner, *this);
    EntityEvents::fireTrigger(owner, *this, slot->get("on_equip"), &player);
    return true;
}

const ItemStack &MobActor::_equippableItem(int32_t index) const {
    if (index == EQUIPPABLE_BODY_SLOT)
        return mEquipment.getSlot(MobEquipment::BODY);
    return mEquipment.getInventoryItem(index);
}

void MobActor::_setEquippableItem(int32_t index, ItemStack item) {
    if (index == EQUIPPABLE_BODY_SLOT)
        mEquipment.setSlot(MobEquipment::BODY, std::move(item));
    else
        mEquipment.setInventoryItem(index, std::move(item));
}

void MobActor::_dropEquipmentSlot(ServerNetworkHandler &owner, const std::string &slotName, float yOffset) {
    Level &level = owner.getLevelFor(*this);
    const Vector3f position = getPosition();
    const Vector3f dropPosition(position.x, position.y + yOffset, position.z);

    const int32_t equipmentSlot = MobEquipment::slotIndexOf(slotName);
    if (equipmentSlot >= 0) {
        if (mEquipment.dropSlot(owner, level, dropPosition, slotName))
            mEquipment.broadcast(owner, *this);
        return;
    }

    const int32_t inventorySlot = (int32_t) std::strtol(slotName.c_str(), nullptr, 10);
    ItemStack item = _equippableItem(inventorySlot);
    if (item.isAir())
        return;

    _setEquippableItem(inventorySlot, ItemStack::air());
    mEquipment.broadcast(owner, *this);
    owner.dropItem(level, dropPosition, item, ItemActorHandler::randomDropMotion(),
                   ItemActorHandler::DROP_PICKUP_DELAY);

    const json::Value *slot = _equippableSlot(inventorySlot);
    if (slot != nullptr)
        EntityEvents::fireTrigger(owner, *this, slot->get("on_unequip"));
}

std::vector<ItemStack> MobActor::rollLoot(ServerNetworkHandler &owner, const std::string &path) {
    std::vector<ItemStack> stacks;
    const std::string prefix = LOOT_TABLE_PREFIX;
    const std::string key = path.rfind(prefix, 0) == 0 ? path.substr(prefix.size()) : path;
    const LootTable *table = LootTableRegistry::getInstance().get(key);
    if (table == nullptr)
        return stacks;

    LootContext context(lootRandom());
    context.mDifficulty = (int32_t) owner.getProperties().getDifficulty();
    context.mColorIndex = (int32_t) numberIn(getComponent("minecraft:color"), "value", 0.0f);
    context.mMarkVariant = (int32_t) numberIn(getComponent("minecraft:mark_variant"), "value", 0.0f);

    for (const LootDrop &drop: table->roll(context)) {
        ItemStack stack = LootItems::toItemStack(owner, drop);
        if (!stack.isAir())
            stacks.push_back(std::move(stack));
    }
    return stacks;
}

void MobActor::_spawnLoot(ServerNetworkHandler &owner, Level &level, const std::string &path) {
    const Vector3f position = getPosition();
    for (const ItemStack &stack: rollLoot(owner, path))
        owner.dropItem(level, position, stack, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
}

void MobActor::_emitInteractParticle(ServerNetworkHandler &owner, const ServerPlayer &player,
                                     const json::Value &particle, const std::string &type) {
    const char *effect = nullptr;
    for (const InteractParticle &entry: INTERACT_PARTICLES) {
        if (type == entry.mType)
            effect = entry.mEffect;
    }
    if (effect == nullptr)
        return;

    Vector3f position = getPosition();
    position.y += getSize().mHeight * mScale * 0.5f + numberIn(&particle, "particle_y_offset", 0.0f);

    const json::Value *towards = particle.get("particle_offset_towards_interactor");
    if (towards != nullptr && towards->boolean(false)) {
        const Vector3f target = player.getPosition();
        const float dx = target.x - position.x;
        const float dz = target.z - position.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        const float reach = getSize().mWidth * mScale * 0.5f;
        if (length > 0.0f) {
            position.x += dx / length * reach;
            position.z += dz / length * reach;
        }
    }

    owner.spawnParticleEffect(owner.getLevelFor(*this), effect, position);
}

void MobActor::_givePlayerItem(ServerNetworkHandler &owner, ServerPlayer &player, ItemStack item) {
    PlayerInventory &inventory = player.getInventory();
    if (inventory.getItemInHand().isAir()) {
        inventory.setItemInHand(std::move(item));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
        return;
    }

    std::vector<int> touched;
    if (inventory.addItem(item, touched)) {
        for (int slot: touched)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        return;
    }

    owner.dropItem(owner.getLevelFor(player), player.getPosition(), item, ItemActorHandler::randomDropMotion(),
                   ItemActorHandler::DROP_PICKUP_DELAY);
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

    mOwnerId = player.getUniqueId();
    mLegacyOwnerName.clear();
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
    if (getComponent("minecraft:sittable") == nullptr || !isOwnedBy(player))
        return false;

    mSitting = !mSitting;
    _setFlag(owner, ActorFlag::Sitting, mSitting);
    return true;
}

int32_t MobActor::getVariant() const {
    return (int32_t) numberIn(getComponent("minecraft:variant"), "value", 0.0f);
}

void MobActor::setVariant(int32_t variant) {
    const json::Value *definition = getDefinition();
    const json::Value *groups = definition == nullptr ? nullptr : definition->get("component_groups");
    if (groups == nullptr || !groups->isObject())
        return;

    std::string match;
    for (const std::string &name: groups->mKeys) {
        const json::Value *component = groups->get(name)->get("minecraft:variant");
        if (component == nullptr)
            continue;

        if ((int32_t) numberIn(component, "value", 0.0f) == variant)
            match = name;
        else
            removeComponentGroup(name);
    }

    if (!match.empty())
        addComponentGroup(match);
}

void MobActor::inheritVariant(const MobActor &firstParent, const MobActor &secondParent) {
    const json::Value *offspring = getComponent("minecraft:offspring");
    const json::Value *breedable = getComponent("minecraft:breedable");
    const json::Value *deny = offspring == nullptr ? nullptr : offspring->get("deny_parents_variant");
    if (deny == nullptr && breedable != nullptr)
        deny = breedable->get("deny_parents_variant");
    if (deny == nullptr)
        return;

    const int32_t firstVariant = firstParent.getVariant();
    const int32_t secondVariant = secondParent.getVariant();
    int32_t variant = std::uniform_int_distribution<int32_t>(0, 1)(lifecycleRandom()) == 0 ? firstVariant
                                                                                           : secondVariant;

    if (firstVariant == secondVariant
        && std::uniform_real_distribution<float>(0.0f, 1.0f)(lifecycleRandom()) < numberIn(deny, "chance", 0.0f)) {
        const int32_t minimum = (int32_t) numberIn(deny, "min_variant", 0.0f);
        const int32_t maximum = (int32_t) numberIn(deny, "max_variant", 0.0f);
        std::vector<int32_t> candidates;
        for (int32_t candidate = minimum; candidate <= maximum; ++candidate) {
            if (candidate != firstVariant)
                candidates.push_back(candidate);
        }
        if (!candidates.empty())
            variant = candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(lifecycleRandom())];
    }

    setVariant(variant);
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
    RideControlSystem::syncFlags(owner, *this);
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

    if (mOwnerId != NO_OWNER) {
        EntityDataEntry entry;
        entry.mId = ActorFlags::OWNER_DATA_ID;
        entry.mFormat = EntityDataFormat::Long;
        entry.mLongValue = mOwnerId;
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
    if (mOwnerId == NO_OWNER && !mLegacyOwnerName.empty()) {
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned() && entry.second.getName() == mLegacyOwnerName) {
                mOwnerId = entry.second.getUniqueId();
                mLegacyOwnerName.clear();
                break;
            }
        }
    }

    if (mOwnerId == mSyncedOwnerId)
        return;

    mSyncedOwnerId = mOwnerId;
    EntityDataMap metadata;
    EntityDataEntry entry;
    entry.mId = ActorFlags::OWNER_DATA_ID;
    entry.mFormat = EntityDataFormat::Long;
    entry.mLongValue = mOwnerId;
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
    return _rolledValue(getComponent("minecraft:movement"), mRolledMovementSpeed, DEFAULT_MOVEMENT_SPEED);
}

float MobActor::getJumpStrength() const {
    return _rolledValue(getComponent(JUMP_STRENGTH_COMPONENT), mRolledJumpStrength, DEFAULT_JUMP_STRENGTH);
}

float MobActor::_rolledValue(const json::Value *component, float &rolled, float fallback) const {
    const json::Value *value = component == nullptr ? nullptr : component->get("value");
    if (value == nullptr)
        return fallback;

    if (!value->isObject())
        return (float) value->number(fallback);

    if (rolled >= 0.0f)
        return rolled;

    const float minimum = numberIn(value, "range_min", fallback);
    const float maximum = numberIn(value, "range_max", minimum);
    rolled = maximum > minimum ? std::uniform_real_distribution<float>(minimum, maximum)(lifecycleRandom()) : minimum;
    return rolled;
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

    std::vector<Tag> definitions;
    definitions.push_back(Tag::ofString(DEFINITION_ADDED_PREFIX + getTypeId()));
    for (const std::string &group: mComponentGroups)
        definitions.push_back(Tag::ofString(DEFINITION_ADDED_PREFIX + group));
    data.put(TAG_DEFINITIONS, Tag::ofList(Tag::Type::String, std::move(definitions)));
    data.putInt(TAG_AGE, mAgeTicks);
    data.putInt(TAG_BREED_COOLDOWN, mBreedCooldown);
    data.putLong(TAG_OWNER, mOwnerId);
    if (!mLegacyOwnerName.empty())
        data.putString(TAG_TAMED_BY, mLegacyOwnerName);
    data.putByte(TAG_SITTING, mSitting ? 1 : 0);
    data.putInt("Variant", (int32_t) numberIn(getComponent("minecraft:variant"), "value", 0.0f));
    data.putInt("MarkVariant", (int32_t) numberIn(getComponent("minecraft:mark_variant"), "value", 0.0f));
    data.putInt("SkinID", (int32_t) numberIn(getComponent("minecraft:skin_id"), "value", 0.0f));
    data.putByte("Color", (int8_t) numberIn(getComponent("minecraft:color"), "value", 0.0f));
    data.putByte("Color2", (int8_t) numberIn(getComponent("minecraft:color2"), "value", 0.0f));
    data.putByte("IsBaby", getComponent("minecraft:is_baby") != nullptr ? 1 : 0);
    data.putByte("IsTamed", getComponent("minecraft:is_tamed") != nullptr ? 1 : 0);
    data.putByte("Saddled", getComponent("minecraft:is_saddled") != nullptr ? 1 : 0);
    data.putByte("Chested", getComponent("minecraft:is_chested") != nullptr ? 1 : 0);
    data.putByte("Sheared", getComponent("minecraft:is_sheared") != nullptr ? 1 : 0);
    data.putInt("InLove", mLoveTicks);
    data.putInt(TAG_TEMPER, mTemper);
    if (mRolledMovementSpeed >= 0.0f)
        data.putFloat(TAG_MOVEMENT_SPEED, mRolledMovementSpeed);
    if (mRolledJumpStrength >= 0.0f)
        data.putFloat(TAG_JUMP_STRENGTH, mRolledJumpStrength);
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
    mOwnerId = data.getLong(TAG_OWNER, NO_OWNER);
    mLegacyOwnerName = mOwnerId == NO_OWNER ? data.getString(TAG_TAMED_BY, std::string()) : std::string();
    mSitting = data.getByte(TAG_SITTING, 0) != 0;
    mLoveTicks = std::max(0, data.getInt("InLove", 0));
    mTemper = data.getInt(TAG_TEMPER, 0);
    mRolledMovementSpeed = data.getFloat(TAG_MOVEMENT_SPEED, attributeIn(data, "minecraft:movement", -1.0f));
    mRolledJumpStrength = data.getFloat(TAG_JUMP_STRENGTH, attributeIn(data, JUMP_STRENGTH_COMPONENT, -1.0f));
    const Tag *home = data.get(TAG_HOME);
    if (home != nullptr && home->isList() && home->getList().size() == 3) {
        const std::vector<Tag> &values = home->getList();
        setHomePosition(Vector3f(values[0].asFloat(), values[1].asFloat(), values[2].asFloat()));
    }
    getFlags().set(ActorFlag::Tamed, isTamed());
    getFlags().set(ActorFlag::Sitting, mSitting);
    mEntitySpawner.loadNbt(data);

    const Tag *definitions = data.get(TAG_DEFINITIONS);
    const Tag *legacyGroups = data.get(TAG_COMPONENT_GROUPS);
    if (definitions != nullptr && definitions->getType() == Tag::Type::List) {
        mComponentGroups.clear();
        for (const Tag &entry: definitions->getList()) {
            const std::string definition = entry.asString();
            if (definition.size() < 2 || definition[0] != DEFINITION_ADDED_PREFIX[0])
                continue;

            const std::string group = definition.substr(1);
            if (group != getTypeId())
                mComponentGroups.push_back(group);
        }
    } else if (legacyGroups != nullptr && legacyGroups->getType() == Tag::Type::List) {
        mComponentGroups.clear();
        for (const Tag &group: legacyGroups->getList())
            mComponentGroups.push_back(group.asString());
    } else {
        return;
    }

    mDefinitionStarted = true;
    _markComponentsChanged();
}

void MobActor::onDamaged(ServerNetworkHandler &owner, Actor *attacker) {
    mLastHurtTick = owner.getCurrentTick();
    mLastHurtBy = attacker != nullptr && attacker != this ? attacker->getRuntimeId() : 0;
    mHurtCount++;
    mAdmiration.abort(owner, *this);
    mAnger.onHurt(owner, *this, attacker != this ? attacker : nullptr);
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
    return owner.getActorByRuntimeId(runtimeId);
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

    MobActor *killer = mLastHurtBy == 0 ? nullptr : dynamic_cast<MobActor *>(findActor(owner, mLastHurtBy));
    if (killer != nullptr)
        killer->onKilledActor(owner, *this);

    if (owner.getLevel().getGameRules().getBool("domobloot")) {
        Level &level = owner.getLevelFor(*this);
        const int experience = _deathExperience(source);
        dropLoot(owner, level, source, lootingLevel);
        mEquipment.dropOnDeath(owner, level, *this, source != nullptr, lootingLevel);

        if (experience > 0)
            owner.spawnExperienceOrbs(level, getPosition(), experience);
    }

    ServerActor::kill(owner, source, lootingLevel);
}

int MobActor::_experienceReward(const char *key, const ServerPlayer *player) const {
    const json::Value *reward = getComponent(EXPERIENCE_REWARD_COMPONENT);
    const json::Value *formula = reward == nullptr ? nullptr : reward->get(key);
    if (formula == nullptr)
        return -1;

    if (!formula->isString())
        return std::max(0, (int) std::lround(formula->number(0.0)));

    MolangContext context;
    context.mLastHitByPlayer = player != nullptr;
    context.mPlayerLevel = player == nullptr ? 0 : player->getExperience().getXpLevel();
    return std::max(0, (int) std::lround(Molang::evaluate(formula->mString, *this, context).mNumber));
}

int MobActor::_deathExperience(const ServerPlayer *killer) const {
    const int reward = _experienceReward("on_death", killer);
    if (reward >= 0)
        return reward;
    return killer == nullptr ? 0 : getExperienceDrop();
}

int MobActor::getBreedingExperience() const {
    return _experienceReward("on_bred", nullptr);
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
