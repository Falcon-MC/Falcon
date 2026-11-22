#include "World/Actor/Mob/Component/MobEffectAura.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <cmath>
#include <vector>

namespace {
    const char *const COMPONENT = "minecraft:mob_effect";
    const float TICKS_PER_SECOND = 20.0f;
    // the component documentation's defaults
    const float DEFAULT_RANGE = 0.2f;
    const float DEFAULT_TIME = 10.0f;
    const float DEFAULT_COOLDOWN = 0.0f;

    float numberOf(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    // a spectator or a dead actor is left alone
    bool canBeAffected(const Actor &actor) {
        if (!actor.isAlive())
            return false;
        const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(&actor);
        return player == nullptr || !player->isSpectator();
    }
}

void MobEffectAura::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(COMPONENT);
    if (component == nullptr)
        return;
    const json::Value *name = component->get("mob_effect");
    MobEffectId effect;
    if (name == nullptr || !parseMobEffect(name->string(), effect))
        return;

    const int64_t now = owner.getCurrentTick();
    const int64_t cooldown = (int64_t) std::lround(numberOf(*component, "cooldown_time", DEFAULT_COOLDOWN)
                                                   * TICKS_PER_SECOND);
    if (cooldown != 0) {
        if (mNextTick.has_value() && now <= *mNextTick)
            return;
        mNextTick = now + cooldown;
    }

    const float range = numberOf(*component, "effect_range", DEFAULT_RANGE);
    const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(mob).expand(range, range, range);
    std::vector<Actor *> targets;
    Level &level = owner.getLevelFor(mob);
    owner.forEachActorNear(mob.getPosition(), range + mob.getSize().mWidth, [&](ServerActor &actor) {
        if (&actor != &mob && &owner.getLevelFor(actor) == &level
            && area.intersectsWith(ActorPushSystem::boundingBoxOf(actor)))
            targets.push_back(&actor);
    });
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.isSpawned() && &owner.getLevelFor(player) == &level
            && area.intersectsWith(ActorPushSystem::boundingBoxOf(player)))
            targets.push_back(&player);
    }

    const json::Value *filter = component->get("entity_filter");
    const json::Value *ambient = component->get("ambient");
    MobEffectInstance instance;
    instance.mId = effect;
    instance.mDuration = (int32_t) std::lround(numberOf(*component, "effect_time", DEFAULT_TIME) * TICKS_PER_SECOND);
    instance.mAmplifier = 0;
    instance.mAmbient = ambient != nullptr && ambient->boolean(false);
    for (Actor *target: targets) {
        if (!canBeAffected(*target))
            continue;
        if (filter != nullptr && !EntityFilter::test(*filter, owner, mob, target))
            continue;
        const ServerActor *server = dynamic_cast<const ServerActor *>(target);
        if (server != nullptr && server->isInvulnerable())
            continue;
        owner.addEffect(*target, instance);
    }
}
