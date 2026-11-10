#include "World/Actor/Mob/Component/MobAreaAttack.h"

#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

#include <vector>

namespace {
    const char *const AREA_ATTACK_COMPONENT = "minecraft:area_attack";
    const char *const DEFAULT_CAUSE = "entity_attack";
    const char *const GENERIC_DEATH = "death.attack.generic";
    const float TICKS_PER_SECOND = 20.0f;
    // reach of the actor index around the attacker, past its own box
    const float SEARCH_MARGIN = 4.0f;
    // sub_147B40020 drops expired cooldowns every 1200 ticks, staggered by the attacker id
    const int64_t COOLDOWN_PURGE_INTERVAL = 1200;

    float numberIn(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }
}

void MobAreaAttack::_read(const json::Value &component) {
    mSettings = Settings();
    mSettings.mRange = numberIn(component, "damage_range", mSettings.mRange);
    // the integer damage per tick, as a float
    mSettings.mDamage = (float) (int32_t) numberIn(component, "damage_per_tick", mSettings.mDamage);
    mSettings.mCooldownTicks = (int64_t) (numberIn(component, "damage_cooldown", 0.0f) * TICKS_PER_SECOND);

    const json::Value *cause = component.get("cause");
    mSettings.mCause = cause == nullptr ? DEFAULT_CAUSE : cause->string();
    const json::Value *message = component.get("death_message_override");
    const char *causeKey = DamageCause::findDeathMessageKey(mSettings.mCause);
    mSettings.mDeathMessageKey = message != nullptr && message->isString() ? message->mString
                                 : causeKey != nullptr                     ? causeKey
                                                                           : GENERIC_DEATH;

    const json::Value *useSelf = component.get("use_self_as_damage_source");
    mSettings.mSelfAsSource = useSelf != nullptr && useSelf->boolean(false);
    mSettings.mFilter = component.get("entity_filter");
}

void MobAreaAttack::tick(ServerNetworkHandler &owner, MobActor &mob) {
    // sub_147B40020
    const json::Value *component = mob.getComponent(AREA_ATTACK_COMPONENT);
    if (component != mComponent) {
        mComponent = component;
        mCooldowns.clear();
        if (component != nullptr)
            _read(*component);
    }
    if (component == nullptr || !mob.isAlive() || mob.isExpired())
        return;

    const int64_t tick = owner.getCurrentTick();
    if (!mCooldowns.empty() && (tick + mob.getUniqueId()) % COOLDOWN_PURGE_INTERVAL == 0) {
        for (auto it = mCooldowns.begin(); it != mCooldowns.end();)
            it = it->second <= tick ? mCooldowns.erase(it) : std::next(it);
    }

    const float range = mSettings.mRange;
    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(mob).expand(range, range, range);
    std::vector<Actor *> targets;
    owner.forEachActorNear(mob.getPosition(), mob.getSize().mWidth + range + SEARCH_MARGIN, [&](ServerActor &other) {
        if (&other != &mob && other.isAlive() && !other.isProjectile() && other.sharesLevelWith(mob)
            && ActorPushSystem::boundingBoxOf(other).intersectsWith(box))
            targets.push_back(&other);
    });
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.isSpawned() && !player.isDead() && player.sharesLevelWith(mob)
            && ActorPushSystem::boundingBoxOf(player).intersectsWith(box))
            targets.push_back(&player);
    }

    for (Actor *target: targets)
        _attack(owner, mob, *target);
}

void MobAreaAttack::_attack(ServerNetworkHandler &owner, MobActor &mob, Actor &target) {
    if (mSettings.mFilter != nullptr && !EntityFilter::test(*mSettings.mFilter, owner, mob, &target))
        return;

    if (mSettings.mCooldownTicks > 0) {
        const int64_t tick = owner.getCurrentTick();
        const auto found = mCooldowns.find(target.getUniqueId());
        if (found != mCooldowns.end() && tick < found->second)
            return;
        mCooldowns[target.getUniqueId()] = tick + mSettings.mCooldownTicks;
    }

    ActorDamageSource source = mSettings.mSelfAsSource
                                       ? ActorDamageSource::attack(mSettings.mDeathMessageKey, target.getName(), mob,
                                                                   mob.getName(), mob.getPosition())
                                       : ActorDamageSource::environment(mSettings.mDeathMessageKey, target.getName());
    source.withCause(mSettings.mCause);

    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&target)) {
        owner.hurt(*player, mSettings.mDamage, source);
        return;
    }
    if (ServerActor *actor = dynamic_cast<ServerActor *>(&target))
        owner.damageActor(*actor, mSettings.mDamage, source);
}
