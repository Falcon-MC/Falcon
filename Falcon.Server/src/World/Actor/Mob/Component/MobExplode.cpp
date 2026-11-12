#include "World/Actor/Mob/Component/MobExplode.h"

#include "World/Actor/ActorFlags.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <random>

namespace {
    const char *const EXPLODE_COMPONENT = "minecraft:explode";
    const float TICKS_PER_SECOND = 20.0f;
    // the documented defaults: fuse_length [0, 0], power 3
    const float DEFAULT_FUSE_SECONDS = 0.0f;
    const float DEFAULT_POWER = 3.0f;

    bool flagIn(const json::Value &component, const char *key, bool fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }

    std::mt19937 &fuseRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float fuseSeconds(const json::Value *fuse) {
        if (fuse == nullptr)
            return DEFAULT_FUSE_SECONDS;
        if (!fuse->isObject())
            return (float) fuse->number(DEFAULT_FUSE_SECONDS);

        const json::Value *minimum = fuse->get("range_min");
        const json::Value *maximum = fuse->get("range_max");
        const float low = minimum == nullptr ? DEFAULT_FUSE_SECONDS : (float) minimum->number(DEFAULT_FUSE_SECONDS);
        const float high = maximum == nullptr ? low : (float) maximum->number(low);
        return high <= low ? low : std::uniform_real_distribution<float>(low, high)(fuseRandom());
    }
}

bool MobExplode::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mExploded)
        return false;

    _sync(owner, mob);
    const json::Value *component = mComponent;
    if (component == nullptr || !mLit)
        return true;

    if (mFuseTicks > 0) {
        --mFuseTicks;
        return true;
    }

    _explode(owner, mob, *component);
    return false;
}

void MobExplode::_sync(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(EXPLODE_COMPONENT);
    if (component == mComponent)
        return;
    mComponent = component;
    mFuseTicks = component == nullptr ? -1 : (int32_t) (fuseSeconds(component->get("fuse_length")) * TICKS_PER_SECOND);
    mLit = component != nullptr && flagIn(*component, "fuse_lit", false);
    _setIgnited(owner, mob, mLit);
}

void MobExplode::setFuseLength(ServerNetworkHandler &owner, MobActor &mob, int32_t ticks) {
    _sync(owner, mob);
    if (mComponent != nullptr)
        mFuseTicks = ticks;
}

void MobExplode::_setIgnited(ServerNetworkHandler &owner, MobActor &mob, bool ignited) {
    // the client draws the swelling fuse from the ignited flag
    if (mob.getFlags().get(ActorFlag::Ignited) == ignited)
        return;

    mob.getFlags().set(ActorFlag::Ignited, ignited);
    owner.syncActorFlags(mob);
}

void MobExplode::_explode(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component) {
    mExploded = true;
    Level &level = owner.getLevelFor(mob);
    const json::Value *power = component.get("power");
    Explosion explosion(owner, level, mob.getPosition(),
                        power == nullptr ? DEFAULT_POWER : power->number(DEFAULT_POWER), &mob, false);
    explosion.setIncendiary(flagIn(component, "causes_fire", false));

    const bool griefing = !flagIn(component, "destroy_affected_by_griefing", false)
                          || level.getGameRules().getBool("mobgriefing");
    // allow_underwater off: under water the blast leaves the blocks
    const bool underwater = LiquidBlocksFetch::at(level, mob.getPosition()).water;
    if (flagIn(component, "breaks_blocks", true) && griefing
        && (!underwater || flagIn(component, "allow_underwater", false)))
        explosion.explode();
    else
        explosion.explodeWithoutBlocks();

    mob.despawn();
}
