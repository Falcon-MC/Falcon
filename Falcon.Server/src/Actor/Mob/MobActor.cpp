#include "Actor/Mob/MobActor.h"

#include "Actor/AI/Goal/BehaviorGoals.h"
#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/Definition/EntityDefinitions.h"
#include "Actor/Definition/EntityEvents.h"
#include "Actor/ServerPlayer.h"
#include "Level/Level.h"
#include "Loot/LootItems.h"
#include "Loot/LootTableRegistry.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <random>

namespace {
    const char *const SPAWNED_EVENT = "minecraft:entity_spawned";
    const char *const BORN_EVENT = "minecraft:entity_born";
    const char *const TAG_COMPONENT_GROUPS = "ComponentGroups";
    const char *const TAG_AGE = "AgeTicks";
    const char *const TAG_BREED_COOLDOWN = "BreedCooldown";
    const char *const TAG_TAMED_BY = "TamedBy";
    const char *const TAG_SITTING = "Sitting";
    const float DEFAULT_MOVEMENT_SPEED = 0.25f;
    const int32_t LOVE_TICKS = 600;
    const int32_t BREED_COOLDOWN_TICKS = 6000;
    const float DEFAULT_AGEABLE_SECONDS = 1200.0f;
    const float DEFAULT_FEED_GROWTH = 0.1f;
    const int32_t TICKS_PER_SECOND = 20;

    std::mt19937 &lifecycleRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    std::string eventOf(const json::Value *trigger) {
        if (trigger == nullptr)
            return std::string();
        if (trigger->isString())
            return trigger->mString;
        const json::Value *event = trigger->get("event");
        return event == nullptr ? std::string() : event->string();
    }

    float numberIn(const json::Value *component, const char *key, float fallback) {
        const json::Value *value = component == nullptr ? nullptr : component->get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
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
        if (getDefinition() != nullptr)
            fireEvent(owner, mBorn ? BORN_EVENT : SPAWNED_EVENT);
    }

    if (!mGoalsRegistered || mGoalsDirty)
        _registerGoals(owner);

    if (mBodyDirty)
        _syncBody(owner);

    _tickLifecycle(owner);

    mGoalSelector.tick(owner, *this);
    mNavigation.tick(owner, *this);
    tickControls(owner);
    ServerActor::tick(owner);
}

void MobActor::_registerGoals(ServerNetworkHandler &owner) {
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

    if (mLoveTicks > 0 && --mLoveTicks == 0)
        _setFlag(owner, ActorFlag::InLove, false);

    const json::Value *ageable = getComponent("minecraft:ageable");
    if (ageable == nullptr || getComponent("minecraft:is_baby") == nullptr)
        return;

    const float duration = numberIn(ageable, "duration", DEFAULT_AGEABLE_SECONDS) * TICKS_PER_SECOND;
    if (++mAgeTicks < (int32_t) duration)
        return;

    mAgeTicks = 0;
    const std::string event = eventOf(ageable->get("grow_up"));
    if (!event.empty())
        fireEvent(owner, event);
}

bool MobActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    const ItemStack held = player.getInventory().getItemInHand();
    if (_tryTame(owner, player, held) || _tryFeedBaby(owner, player, held) || _tryStartLove(owner, player, held)
        || _trySit(owner, player))
        return true;

    return ServerActor::onInteract(owner, player);
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
    owner.broadcastActorEvent(*this, EntityEventType::TamingSucceeded);

    const std::string event = eventOf(tameable->get("tame_event"));
    if (!event.empty())
        fireEvent(owner, event);
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
    if (getDefinition() == nullptr)
        return;

    const bool baby = getComponent("minecraft:is_baby") != nullptr;
    if (getFlags().get(ActorFlag::Baby) != baby) {
        getFlags().set(ActorFlag::Baby, baby);
        owner.syncActorFlags(*this);
    }

    const json::Value *scaleComponent = getComponent("minecraft:scale");
    const json::Value *scaleValue = scaleComponent == nullptr ? nullptr : scaleComponent->get("value");
    const float scale = scaleValue == nullptr ? 1.0f : (float) scaleValue->number(1.0);
    if (scale != mScale) {
        mScale = scale;
        owner.syncActorScale(*this, scale);
    }
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

void MobActor::fireEvent(ServerNetworkHandler &owner, const std::string &event) {
    EntityEvents::fire(owner, *this, event);
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

    return data;
}

void MobActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);

    mAgeTicks = data.getInt(TAG_AGE, 0);
    mBreedCooldown = data.getInt(TAG_BREED_COOLDOWN, 0);
    mTamedBy = data.getString(TAG_TAMED_BY, std::string());
    mSitting = data.getByte(TAG_SITTING, 0) != 0;
    getFlags().set(ActorFlag::Tamed, !mTamedBy.empty());
    getFlags().set(ActorFlag::Sitting, mSitting);

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
    mLastHurtBy = attacker != nullptr && attacker->isPlayer() ? attacker->getRuntimeId() : 0;
    mHurtCount++;
}

ServerPlayer *MobActor::getTarget(ServerNetworkHandler &owner) const {
    if (mTargetRuntimeId == 0)
        return nullptr;

    ServerPlayer *player = findPlayer(owner, mTargetRuntimeId);
    return player != nullptr && canTarget(*player) ? player : nullptr;
}

bool MobActor::canTarget(const ServerPlayer &player) const {
    if (!player.isSpawned() || player.isDead() || player.getDimension() != getDimension())
        return false;

    const int32_t gameType = player.getGameType();
    return gameType != (int32_t) GameType::Creative && gameType != (int32_t) GameType::Spectator;
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
