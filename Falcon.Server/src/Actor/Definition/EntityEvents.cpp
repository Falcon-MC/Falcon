#include "Actor/Definition/EntityEvents.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"

#include <algorithm>
#include <random>

namespace {
    const int32_t MAX_DEPTH = 16;

    std::mt19937 &eventRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

void EntityEvents::fire(ServerNetworkHandler &owner, MobActor &mob, const std::string &event, int32_t depth) {
    const json::Value *definition = mob.getDefinition();
    const json::Value *events = definition == nullptr ? nullptr : definition->get("events");
    const json::Value *node = events == nullptr ? nullptr : events->get(event);
    if (node != nullptr)
        _run(owner, mob, *node, depth);
}

void EntityEvents::_applyGroups(MobActor &mob, const json::Value &change, bool add) {
    const json::Value *groups = change.get("component_groups");
    if (groups == nullptr)
        return;

    for (const std::unique_ptr<json::Value> &group: groups->mArray) {
        if (add)
            mob.addComponentGroup(group->string());
        else
            mob.removeComponentGroup(group->string());
    }
}

bool EntityEvents::_run(ServerNetworkHandler &owner, MobActor &mob, const json::Value &node, int32_t depth) {
    if (depth > MAX_DEPTH || !node.isObject())
        return false;

    if (const json::Value *filters = node.get("filters")) {
        if (!EntityFilter::test(*filters, owner, mob))
            return false;
    }

    if (const json::Value *remove = node.get("remove"))
        _applyGroups(mob, *remove, false);
    if (const json::Value *add = node.get("add"))
        _applyGroups(mob, *add, true);

    if (const json::Value *sequence = node.get("sequence")) {
        for (const std::unique_ptr<json::Value> &entry: sequence->mArray)
            _run(owner, mob, *entry, depth + 1);
    }

    if (const json::Value *firstValid = node.get("first_valid")) {
        for (const std::unique_ptr<json::Value> &entry: firstValid->mArray) {
            if (_run(owner, mob, *entry, depth + 1))
                break;
        }
    }

    if (const json::Value *randomize = node.get("randomize")) {
        int32_t total = 0;
        for (const std::unique_ptr<json::Value> &entry: randomize->mArray)
            total += entry->get("weight") != nullptr ? std::max(0, entry->get("weight")->integer(1)) : 1;

        if (total > 0) {
            int32_t roll = std::uniform_int_distribution<int32_t>(0, total - 1)(eventRandom());
            for (const std::unique_ptr<json::Value> &entry: randomize->mArray) {
                roll -= entry->get("weight") != nullptr ? std::max(0, entry->get("weight")->integer(1)) : 1;
                if (roll < 0) {
                    _run(owner, mob, *entry, depth + 1);
                    break;
                }
            }
        }
    }

    if (const json::Value *trigger = node.get("trigger")) {
        const std::string event = trigger->isString() ? trigger->mString
                                                      : (trigger->get("event") != nullptr
                                                         ? trigger->get("event")->string() : std::string());
        const json::Value *target = trigger->isObject() ? trigger->get("target") : nullptr;
        if (!event.empty() && (target == nullptr || target->string() == "self"))
            fire(owner, mob, event, depth + 1);
    }

    return true;
}
