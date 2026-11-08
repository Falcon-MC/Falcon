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
    const float TICKS_PER_SECOND = 20.0f;
    // reach of the actor index around the attacker, past its own box
    const float SEARCH_MARGIN = 4.0f;

    float numberIn(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }
}

void MobAreaAttack::tick(ServerNetworkHandler &owner, MobActor &mob) {
    // sub_147B40020
    const json::Value *component = mob.getComponent(AREA_ATTACK_COMPONENT);
    if (component == nullptr) {
        mCooldowns.clear();
        return;
    }
    if (!mob.isAlive() || mob.isExpired())
        return;

    const float range = numberIn(*component, "damage_range", 0.2f);
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
        _attack(owner, mob, *component, *target);
}

void MobAreaAttack::_attack(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component, Actor &target) {
    const json::Value *filter = component.get("entity_filter");
    if (filter != nullptr && !EntityFilter::test(*filter, owner, mob, &target))
        return;

    const int64_t cooldown = (int64_t) (numberIn(component, "damage_cooldown", 0.0f) * TICKS_PER_SECOND);
    const int64_t tick = owner.getCurrentTick();
    if (cooldown > 0) {
        const auto found = mCooldowns.find(target.getUniqueId());
        if (found != mCooldowns.end() && tick < found->second)
            return;
        mCooldowns[target.getUniqueId()] = tick + cooldown;
    }

    const json::Value *causeValue = component.get("cause");
    const std::string cause = causeValue == nullptr ? DEFAULT_CAUSE : causeValue->string();
    const json::Value *message = component.get("death_message_override");
    const char *causeKey = DamageCause::findDeathMessageKey(cause);
    const std::string deathKey = message != nullptr && message->isString() ? message->mString
                                 : causeKey != nullptr ? causeKey : "death.attack.generic";

    const json::Value *useSelf = component.get("use_self_as_damage_source");
    ActorDamageSource source = useSelf != nullptr && useSelf->boolean(false)
                               ? ActorDamageSource::attack(deathKey, target.getName(), mob, mob.getName(),
                                                           mob.getPosition())
                               : ActorDamageSource::environment(deathKey, target.getName());
    source.withCause(cause);

    // the integer damage per tick, as a float
    const float damage = (float) (int32_t) numberIn(component, "damage_per_tick", 2.0f);
    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&target)) {
        owner.hurt(*player, damage, source);
        return;
    }
    if (ServerActor *actor = dynamic_cast<ServerActor *>(&target))
        owner.damageActor(*actor, damage, source);
}
