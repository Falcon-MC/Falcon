#include "Actor/Definition/BlockSensor.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Block/BlockState.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <vector>

namespace {
    const char *const BLOCK_SENSOR_COMPONENT = "minecraft:block_sensor";
    const float DEFAULT_SENSOR_RADIUS = 8.0f;

    bool listContains(const json::Value *list, const std::string &block) {
        if (list == nullptr)
            return false;

        for (const std::unique_ptr<json::Value> &entry: list->mArray) {
            const std::string name = entry->string();
            if (name == block || "minecraft:" + name == block)
                return true;
        }
        return false;
    }
}

void BlockSensor::onBlockBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state, Actor &source) {
    std::vector<MobActor *> sensing;
    for (auto &entry: owner.getActors()) {
        MobActor *mob = dynamic_cast<MobActor *>(entry.second.get());
        if (mob == nullptr || !mob->isAlive() || mob->isExpired() || &owner.getLevelFor(*mob) != &level)
            continue;

        if (mob->getComponent(BLOCK_SENSOR_COMPONENT) != nullptr)
            sensing.push_back(mob);
    }

    const std::string block = state.mName;
    for (MobActor *mob: sensing) {
        const json::Value *sensor = mob->getComponent(BLOCK_SENSOR_COMPONENT);
        if (sensor != nullptr)
            _sense(owner, *mob, *sensor, position, block, source);
    }
}

void BlockSensor::_sense(ServerNetworkHandler &owner, MobActor &mob, const json::Value &sensor,
                         const Vector3i &position, const std::string &block, Actor &source) {
    const json::Value *radiusValue = sensor.get("sensor_radius");
    const float radius = radiusValue == nullptr ? DEFAULT_SENSOR_RADIUS
                                                : (float) radiusValue->number(DEFAULT_SENSOR_RADIUS);
    const Vector3f mobPosition = mob.getPosition();
    const float dx = (float) position.x + 0.5f - mobPosition.x;
    const float dy = (float) position.y + 0.5f - mobPosition.y;
    const float dz = (float) position.z + 0.5f - mobPosition.z;
    if (dx * dx + dy * dy + dz * dz > radius * radius || !_matchesSource(owner, mob, sensor, source))
        return;

    const json::Value *onBreak = sensor.get("on_break");
    if (onBreak == nullptr)
        return;

    for (const std::unique_ptr<json::Value> &entry: onBreak->mArray) {
        const json::Value *event = entry->get("on_block_broken");
        if (event == nullptr || event->string().empty() || !listContains(entry->get("block_list"), block))
            continue;

        mob.setEventBlock(position);
        mob.fireEvent(owner, event->string(), &source);
        mob.clearEventBlock();
    }
}

bool BlockSensor::_matchesSource(ServerNetworkHandler &owner, MobActor &mob, const json::Value &sensor,
                                 Actor &source) {
    const json::Value *sources = sensor.get("sources");
    if (sources == nullptr || (sources->isArray() && sources->mArray.empty()))
        return true;

    if (!sources->isArray())
        return EntityFilter::test(*sources, owner, mob, &source);

    for (const std::unique_ptr<json::Value> &filter: sources->mArray) {
        if (EntityFilter::test(*filter, owner, mob, &source))
            return true;
    }
    return false;
}
