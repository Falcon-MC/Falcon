#include "Actor/Mob/MobEntitySpawner.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace {
    const char *const SPAWN_ENTITY_COMPONENT = "minecraft:spawn_entity";
    const char *const TAG_SPAWN_TIMERS = "SpawnEntityTimers";
    const char *const NAMESPACE = "minecraft:";
    const char *const DEFAULT_SPAWN_ITEM = "egg";
    const char *const DEFAULT_SPAWN_EVENT = "minecraft:entity_born";
    const float DEFAULT_MIN_WAIT_SECONDS = 300.0f;
    const float DEFAULT_MAX_WAIT_SECONDS = 600.0f;
    const int32_t TICKS_PER_SECOND = 20;
    const int32_t CONSUMED = -1;

    std::mt19937 &spawnRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    std::vector<const json::Value *> entriesOf(const json::Value &component) {
        std::vector<const json::Value *> entries;
        const json::Value *list = component.get("entities");
        if (list == nullptr)
            return entries;

        if (!list->isArray()) {
            entries.push_back(list);
            return entries;
        }

        for (const std::unique_ptr<json::Value> &entry: list->mArray)
            entries.push_back(entry.get());
        return entries;
    }

    float secondsOf(const json::Value &entry, const char *key, float fallback) {
        const json::Value *value = entry.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    std::string textOf(const json::Value &entry, const char *key, const char *fallback) {
        const json::Value *value = entry.get(key);
        return value == nullptr ? std::string(fallback) : value->string();
    }

    int32_t waitTicks(const json::Value &entry) {
        const int32_t minimum = (int32_t) std::lround(secondsOf(entry, "min_wait_time", DEFAULT_MIN_WAIT_SECONDS)
                                                      * (float) TICKS_PER_SECOND);
        const int32_t maximum = (int32_t) std::lround(secondsOf(entry, "max_wait_time", DEFAULT_MAX_WAIT_SECONDS)
                                                      * (float) TICKS_PER_SECOND);
        if (maximum <= minimum)
            return std::max(0, minimum);

        return std::uniform_int_distribution<int32_t>(std::max(0, minimum), maximum)(spawnRandom());
    }

    bool isSingleUse(const json::Value &entry) {
        const json::Value *value = entry.get("single_use");
        return value != nullptr && value->boolean(false);
    }

    std::string actorIdentifier(const std::string &name) {
        return name.find(':') == std::string::npos ? NAMESPACE + name : name;
    }
}

void MobEntitySpawner::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(SPAWN_ENTITY_COMPONENT);
    const int64_t currentTick = owner.getCurrentTick();
    if (component == mComponent && currentTick == mLastTick)
        return;

    mLastTick = currentTick;
    if (component != mComponent)
        _start(component);

    if (component == nullptr)
        return;

    const std::vector<const json::Value *> entries = entriesOf(*component);
    for (size_t index = 0; index < entries.size() && index < mTicks.size(); ++index) {
        if (mTicks[index] == CONSUMED)
            continue;

        if (mTicks[index] > 0) {
            --mTicks[index];
            continue;
        }

        const json::Value &entry = *entries[index];
        const json::Value *filters = entry.get("filters");
        if (filters == nullptr || EntityFilter::test(*filters, owner, mob))
            _spawn(owner, mob, entry);

        mTicks[index] = isSingleUse(entry) ? CONSUMED : waitTicks(entry);
    }
}

void MobEntitySpawner::_start(const json::Value *component) {
    mComponent = component;
    mTicks.clear();
    if (component != nullptr) {
        for (const json::Value *entry: entriesOf(*component))
            mTicks.push_back(waitTicks(*entry));
    }

    if (!mSavedTicks.empty() && mSavedTicks.size() == mTicks.size())
        mTicks = mSavedTicks;
    mSavedTicks.clear();
}

void MobEntitySpawner::_spawn(ServerNetworkHandler &owner, MobActor &mob, const json::Value &entry) {
    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    const json::Value *countValue = entry.get("num_to_spawn");
    const int32_t count = std::max(1, countValue == nullptr ? 1 : countValue->integer(1));

    const std::string entity = textOf(entry, "spawn_entity", "");
    if (!entity.empty()) {
        const std::string event = textOf(entry, "spawn_event", DEFAULT_SPAWN_EVENT);
        const auto configure = [&event](ServerActor &actor) {
            MobActor *child = dynamic_cast<MobActor *>(&actor);
            if (child != nullptr)
                child->setSpawnEvent(event);
        };

        for (int32_t spawned = 0; spawned < count; ++spawned)
            owner.spawnActor(level, actorIdentifier(entity), position, configure);
    } else {
        const std::string item = textOf(entry, "spawn_item", DEFAULT_SPAWN_ITEM);
        for (int32_t spawned = 0; spawned < count; ++spawned)
            owner.spawnItemActor(level, item, 1, position);
    }

    mob.playDefinitionSound(owner, textOf(entry, "spawn_sound", LevelSoundEvent::PLOP));

    if (entity.empty())
        EntityEvents::fireTrigger(owner, mob, entry.get("spawn_item_event"));
}

void MobEntitySpawner::saveNbt(Tag &data) const {
    const std::vector<int32_t> &ticks = mComponent != nullptr ? mTicks : mSavedTicks;
    if (!ticks.empty())
        data.put(TAG_SPAWN_TIMERS, Tag::ofIntArray(ticks));
}

void MobEntitySpawner::loadNbt(const Tag &data) {
    const Tag *timers = data.get(TAG_SPAWN_TIMERS);
    mSavedTicks.clear();
    if (timers != nullptr && timers->getType() == Tag::Type::IntArray)
        mSavedTicks = timers->asIntArray();

    mComponent = nullptr;
    mTicks.clear();
    mLastTick = -1;
}
