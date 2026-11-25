#include "World/Actor/Mob/Component/MobAngerLevel.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Definition/Molang.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>

namespace {
    const char *const COMPONENT = "minecraft:anger_level";
    const char *const TAG_NUISANCES = "Nuisances";
    const char *const TAG_ACTOR_ID = "ActorId";
    const char *const TAG_ANGER = "Anger";
    const char *const TAG_PRIORITY = "Priority";
    const int32_t TICKS_PER_SECOND = 20;

    // the documented defaults of the component
    const float DEFAULT_DECREMENT_SECONDS = 1.0f;
    const int32_t DEFAULT_MAX_ANGER = 100;
    const int32_t DEFAULT_ANGRY_THRESHOLD = 80;
    const int32_t DEFAULT_ANGRY_BOOST = 20;

    float numberOf(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    bool flagOf(const json::Value &component, const char *key, bool fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }

    int32_t settingOf(const MobActor &mob, const char *key, int32_t fallback) {
        const json::Value *component = mob.getComponent(COMPONENT);
        return component == nullptr ? fallback : (int32_t) numberOf(*component, key, (float) fallback);
    }

    // an entry goes before another when it is angry and the other is not, then players first, then the angrier
    bool comesBefore(const MobAngerLevel::Nuisance &entry, const MobAngerLevel::Nuisance &other, int32_t threshold) {
        if (entry.mAnger >= threshold)
            return other.mAnger < threshold || entry.mPriority < other.mPriority
                   || (entry.mPriority <= other.mPriority && entry.mAnger > other.mAnger);
        return other.mAnger < threshold
               && (entry.mPriority < other.mPriority
                   || (entry.mAnger > other.mAnger && entry.mPriority <= other.mPriority));
    }
}

int32_t MobAngerLevel::defaultAnnoyingness(const MobActor &mob) {
    return settingOf(mob, "default_annoyingness", 0);
}

int32_t MobAngerLevel::projectileAnnoyingness(const MobActor &mob) {
    return settingOf(mob, "default_projectile_annoyingness", 0);
}

int32_t MobAngerLevel::angryThreshold(const MobActor &mob) {
    return settingOf(mob, "angry_threshold", DEFAULT_ANGRY_THRESHOLD);
}

int32_t MobAngerLevel::angryBoost(const MobActor &mob) {
    return settingOf(mob, "angry_boost", DEFAULT_ANGRY_BOOST);
}

int32_t MobAngerLevel::getAngerAt(int64_t actorId) const {
    for (const Nuisance &nuisance: mNuisances) {
        if (nuisance.mActorId == actorId)
            return nuisance.mAnger;
    }
    return 0;
}

bool MobAngerLevel::isValidTarget(ServerNetworkHandler &owner, const MobActor &mob, const Actor &target) const {
    if (!target.isAlive())
        return false;

    const json::Value *component = mob.getComponent(COMPONENT);
    const json::Value *filter = component == nullptr ? nullptr : component->get("nuisance_filter");
    if (filter != nullptr && !EntityFilter::test(*filter, owner, mob, &target))
        return false;

    const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(&target);
    if (player == nullptr)
        return dynamic_cast<const MobActor *>(&target) != nullptr;
    // a player who can be hurt: survival or adventure
    return player->isSurvival() || player->isAdventure();
}

Actor *MobAngerLevel::getActiveEntity(ServerNetworkHandler &owner, const MobActor &mob) const {
    for (const Nuisance &nuisance: mNuisances) {
        Actor *actor = owner.getEntity(nuisance.mActorId);
        if (actor != nullptr && isValidTarget(owner, mob, *actor))
            return actor;
    }
    return nullptr;
}

void MobAngerLevel::_insert(const Nuisance &nuisance, int32_t threshold) {
    auto at = mNuisances.begin();
    while (at != mNuisances.end() && !comesBefore(nuisance, *at, threshold))
        ++at;
    mNuisances.insert(at, nuisance);
}

void MobAngerLevel::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(COMPONENT);
    if (component == nullptr)
        return;

    const int64_t now = owner.getCurrentTick();
    const int64_t interval = std::max<int64_t>(
            1, (int64_t) std::lround(numberOf(*component, "anger_decrement_interval", DEFAULT_DECREMENT_SECONDS)
                                     * TICKS_PER_SECOND));
    if (mNextDecrementTick < 0)
        mNextDecrementTick = now + interval;
    while (now >= mNextDecrementTick) {
        _decrement(owner, mob);
        mNextDecrementTick += interval;
    }

    // a target the mob is no longer angry at is dropped
    if (!flagOf(*component, "remove_targets_below_angry_threshold", true))
        return;
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;
    const int64_t id = target->getUniqueId();
    const auto entry = std::find_if(mNuisances.begin(), mNuisances.end(),
                                    [id](const Nuisance &nuisance) { return nuisance.mActorId == id; });
    if (entry == mNuisances.end() || entry->mAnger < angryThreshold(mob))
        mob.setTarget(owner, 0);
}

void MobAngerLevel::_decrement(ServerNetworkHandler &owner, MobActor &mob) {
    const int32_t threshold = angryThreshold(mob);
    std::vector<Nuisance> kept;
    kept.reserve(mNuisances.size());
    int32_t level = 0;
    for (const Nuisance &nuisance: mNuisances) {
        // an unloaded actor is kept and cools down; a loaded one that can no longer be a target goes
        const Actor *actor = owner.getEntity(nuisance.mActorId);
        if (actor != nullptr) {
            if (!isValidTarget(owner, mob, *actor))
                continue;
            level = std::max(level, nuisance.mAnger - 1);
        }
        if (nuisance.mAnger > 0)
            kept.push_back({nuisance.mActorId, nuisance.mAnger - 1, nuisance.mPriority});
    }

    mNuisances.clear();
    for (const Nuisance &nuisance: kept)
        _insert(nuisance, threshold);
    mLevel = level;
}

void MobAngerLevel::increase(ServerNetworkHandler &owner, MobActor &mob, Actor &target, int32_t amount,
                             bool playSound) {
    const json::Value *component = mob.getComponent(COMPONENT);
    if (component == nullptr || !isValidTarget(owner, mob, target))
        return;

    const int64_t id = target.getUniqueId();
    const auto existing = std::find_if(mNuisances.begin(), mNuisances.end(),
                                       [id](const Nuisance &nuisance) { return nuisance.mActorId == id; });
    const int32_t current = existing == mNuisances.end() ? 0 : existing->mAnger;
    const bool priority = existing == mNuisances.end() ? !target.isPlayer() : existing->mPriority;
    const int32_t maxAnger = (int32_t) numberOf(*component, "max_anger", (float) DEFAULT_MAX_ANGER);
    const int32_t anger = std::clamp(current + amount, 0, maxAnger);
    mLevel = std::max(mLevel, anger);

    if (anger != current) {
        if (existing != mNuisances.end())
            mNuisances.erase(existing);
        if (anger > 0)
            _insert({id, anger, priority}, angryThreshold(mob));
    }

    if (playSound)
        _playIncreaseSound(owner, mob, *component);

    // an angry player takes over from a target that is not one
    const Actor *currentTarget = mob.getTarget(owner);
    if (currentTarget == nullptr || currentTarget->isPlayer() || !target.isPlayer())
        return;
    if (getAngerAt(id) >= angryThreshold(mob))
        mob.setTarget(owner, 0);
}

void MobAngerLevel::_playIncreaseSound(ServerNetworkHandler &owner, MobActor &mob,
                                       const json::Value &component) const {
    const json::Value *sounds = component.get("on_increase_sounds");
    if (sounds == nullptr || !sounds->isArray())
        return;

    for (const std::unique_ptr<json::Value> &entry: sounds->mArray) {
        const json::Value *sound = entry->get("sound");
        if (sound == nullptr)
            continue;
        const json::Value *condition = entry->get("condition");
        if (condition != nullptr && !(Molang::evaluate(condition->string(), mob).mNumber > 0.0))
            continue;
        owner.playLevelSound(owner.getLevelFor(mob), sound->string(), mob.getPosition(), mob.getIdentifier());
        return;
    }
}

void MobAngerLevel::save(Tag &data) const {
    std::vector<Tag> nuisances;
    for (const Nuisance &nuisance: mNuisances) {
        Tag entry = Tag::ofCompound();
        entry.putLong(TAG_ACTOR_ID, nuisance.mActorId);
        entry.putInt(TAG_ANGER, nuisance.mAnger);
        entry.putByte(TAG_PRIORITY, nuisance.mPriority ? 1 : 0);
        nuisances.push_back(std::move(entry));
    }
    if (!nuisances.empty())
        data.put(TAG_NUISANCES, Tag::ofList(Tag::Type::Compound, std::move(nuisances)));
}

void MobAngerLevel::load(const Tag &data) {
    mNuisances.clear();
    const Tag *nuisances = data.get(TAG_NUISANCES);
    if (nuisances == nullptr || !nuisances->isList())
        return;
    // saved in their order
    for (const Tag &entry: nuisances->getList()) {
        if (entry.isCompound())
            mNuisances.push_back({entry.getLong(TAG_ACTOR_ID, -1), entry.getInt(TAG_ANGER, 0),
                                  entry.getByte(TAG_PRIORITY, 0) != 0});
    }
}
