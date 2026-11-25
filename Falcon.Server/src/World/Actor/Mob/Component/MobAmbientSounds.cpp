#include "World/Actor/Mob/Component/MobAmbientSounds.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/EntityDataMap.h"
#include "World/Actor/Definition/Molang.h"
#include "World/Actor/Mob/MobActor.h"

#include <algorithm>
#include <cmath>

namespace {
    const char *const HEARTBEAT_COMPONENT = "minecraft:heartbeat";
    const char *const AMBIENT_COMPONENT = "minecraft:ambient_sound_interval";
    const float TICKS_PER_SECOND = 20.0f;
    // the component documentation's defaults
    const float DEFAULT_AMBIENT_INTERVAL = 8.0f;
    const float DEFAULT_AMBIENT_RANGE = 16.0f;
    const char *const DEFAULT_AMBIENT_EVENT = "ambient";
    const char *const DEFAULT_HEARTBEAT_INTERVAL = "1.00";
    // the client never waits less than a second between two ambient sounds
    const int32_t MIN_AMBIENT_TICKS = 20;
    const double CONDITION_EPSILON = 1.1920929e-07;

    float numberOf(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    EntityDataEntry floatEntry(int32_t id, float value) {
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::Float;
        entry.mFloatValue = value;
        return entry;
    }

    EntityDataEntry intEntry(int32_t id, int32_t value) {
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::Int;
        entry.mIntValue = value;
        return entry;
    }

    EntityDataEntry stringEntry(int32_t id, const std::string &value) {
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::String;
        entry.mStringValue = value;
        return entry;
    }

    void send(ServerNetworkHandler &owner, MobActor &mob, const EntityDataEntry &entry) {
        EntityDataMap metadata;
        metadata.mEntries.push_back(entry);
        owner.sendActorMetadata(mob, metadata);
    }
}

int32_t MobAmbientSounds::heartbeatTicksOf(const MobActor &mob) {
    const json::Value *component = mob.getComponent(HEARTBEAT_COMPONENT);
    if (component == nullptr)
        return 0;
    const json::Value *interval = component->get("interval");
    const std::string expression = interval == nullptr ? DEFAULT_HEARTBEAT_INTERVAL
                                   : interval->isString() ? interval->string()
                                                          : std::to_string(interval->number(1.0));
    const int32_t ticks = (int32_t) std::round((float) Molang::evaluate(expression, mob).mNumber * TICKS_PER_SECOND);
    return std::max(0, ticks);
}

std::string MobAmbientSounds::ambientEventOf(const MobActor &mob) {
    const json::Value *component = mob.getComponent(AMBIENT_COMPONENT);
    if (component == nullptr)
        return std::string();
    if (const json::Value *events = component->get("event_names")) {
        if (events->isArray()) {
            for (const std::unique_ptr<json::Value> &entry: events->mArray) {
                const json::Value *condition = entry->get("condition");
                const json::Value *name = entry->get("event_name");
                if (condition != nullptr && name != nullptr
                    && Molang::evaluate(condition->string(), mob).mNumber > CONDITION_EPSILON)
                    return name->string();
            }
        }
    }
    const json::Value *name = component->get("event_name");
    return name == nullptr ? std::string(DEFAULT_AMBIENT_EVENT) : name->string();
}

void MobAmbientSounds::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mStarted) {
        // the spawn metadata carries the first values
        mStarted = true;
        mHeartbeatTicks = heartbeatTicksOf(mob);
        mAmbientEvent = ambientEventOf(mob);
    }

    if (mob.getComponent(HEARTBEAT_COMPONENT) != nullptr) {
        const int32_t ticks = heartbeatTicksOf(mob);
        if (ticks != mHeartbeatTicks) {
            mHeartbeatTicks = ticks;
            send(owner, mob, intEntry(HEARTBEAT_INTERVAL_DATA_ID, ticks));
        }
    }

    const json::Value *ambient = mob.getComponent(AMBIENT_COMPONENT);
    if (ambient == nullptr)
        return;
    // the event name is picked again once per interval
    const int32_t interval = std::max(
            MIN_AMBIENT_TICKS,
            (int32_t) (numberOf(*ambient, "value", DEFAULT_AMBIENT_INTERVAL) * TICKS_PER_SECOND));
    if (++mAmbientTicks <= interval)
        return;
    mAmbientTicks = 0;
    const std::string event = ambientEventOf(mob);
    if (event == mAmbientEvent)
        return;
    mAmbientEvent = event;
    send(owner, mob, stringEntry(AMBIENT_EVENT_DATA_ID, event));
}

void MobAmbientSounds::fillSpawnMetadata(const MobActor &mob, EntityDataMap &metadata) const {
    if (mob.getComponent(HEARTBEAT_COMPONENT) != nullptr)
        metadata.mEntries.push_back(
                intEntry(HEARTBEAT_INTERVAL_DATA_ID, mStarted ? mHeartbeatTicks : heartbeatTicksOf(mob)));

    const json::Value *ambient = mob.getComponent(AMBIENT_COMPONENT);
    if (ambient == nullptr)
        return;
    metadata.mEntries.push_back(
            floatEntry(AMBIENT_INTERVAL_DATA_ID, numberOf(*ambient, "value", DEFAULT_AMBIENT_INTERVAL)));
    metadata.mEntries.push_back(floatEntry(AMBIENT_RANGE_DATA_ID, numberOf(*ambient, "range", DEFAULT_AMBIENT_RANGE)));
    metadata.mEntries.push_back(stringEntry(AMBIENT_EVENT_DATA_ID, mStarted ? mAmbientEvent : ambientEventOf(mob)));
}
