#include "Actor/Definition/EntityEvents.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    const int32_t MAX_DEPTH = 16;
    const char *const SELF_TARGET = "self";
    const char *const BLOCK_TARGET = "block";

    Vector3i blockAt(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }

    std::mt19937 &eventRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    bool flagOr(const json::Value &options, const char *key, bool fallback) {
        const json::Value *value = options.get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }
}

void EntityEvents::fire(ServerNetworkHandler &owner, MobActor &mob, const std::string &event, int32_t depth,
                        Actor *other) {
    const json::Value *definition = mob.getDefinition();
    const json::Value *events = definition == nullptr ? nullptr : definition->get("events");
    const json::Value *node = events == nullptr ? nullptr : events->get(event);
    if (node != nullptr)
        _run(owner, mob, *node, depth, other);
}

void EntityEvents::fireTrigger(ServerNetworkHandler &owner, MobActor &mob, const json::Value *trigger, Actor *other,
                               int32_t depth) {
    if (trigger == nullptr)
        return;

    if (trigger->isString()) {
        if (!trigger->mString.empty())
            fire(owner, mob, trigger->mString, depth, other);
        return;
    }

    const json::Value *event = trigger->get("event");
    if (event == nullptr || event->string().empty())
        return;

    const json::Value *target = trigger->get("target");
    if (target != nullptr && target->string() == BLOCK_TARGET) {
        if (mob.hasEventBlock())
            mob.fireBlockEvent(owner, mob.getEventBlock(), event->string());
        return;
    }

    MobActor *receiver = _resolveTarget(owner, mob, target == nullptr ? SELF_TARGET : target->string(), other);
    if (receiver != nullptr)
        fire(owner, *receiver, event->string(), depth, receiver == &mob ? other : &mob);
}

void EntityEvents::fireTriggers(ServerNetworkHandler &owner, MobActor &mob, const json::Value *triggers,
                                Actor *other) {
    if (triggers == nullptr)
        return;

    const auto run = [&owner, &mob, other](const json::Value &trigger) {
        const json::Value *filters = trigger.isObject() ? trigger.get("filters") : nullptr;
        if (filters == nullptr || EntityFilter::test(*filters, owner, mob, other))
            fireTrigger(owner, mob, &trigger, other);
    };

    if (!triggers->isArray()) {
        run(*triggers);
        return;
    }

    for (const std::unique_ptr<json::Value> &trigger: triggers->mArray)
        run(*trigger);
}

MobActor *EntityEvents::_resolveTarget(ServerNetworkHandler &owner, MobActor &mob, const std::string &target,
                                       Actor *other) {
    if (target.empty() || target == SELF_TARGET)
        return &mob;
    if (target == "other")
        return dynamic_cast<MobActor *>(other);
    if (target == "target")
        return dynamic_cast<MobActor *>(mob.getTarget(owner));
    if (target == "parent" && mob.getParentRuntimeId() != 0)
        return dynamic_cast<MobActor *>(MobActor::findActor(owner, mob.getParentRuntimeId()));
    return nullptr;
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

void EntityEvents::_setProperties(ServerNetworkHandler &owner, MobActor &mob, const json::Value &properties) {
    if (!properties.isObject())
        return;

    bool changed = false;
    for (const std::string &name: properties.mKeys) {
        const ActorPropertyDescription *descriptor = mob.findPropertyDescription(name);
        const json::Value *value = properties.get(name);
        if (descriptor != nullptr && value != nullptr && mob.assignProperty(*descriptor, *value))
            changed = true;
    }

    if (changed)
        owner.syncActorProperties(mob);
}

void EntityEvents::_stopMovement(MobActor &mob, const json::Value &options) {
    mob.getNavigation().stop(mob);

    Vector3f motion = mob.getMotion();
    if (flagOr(options, "stop_horizontal_movement", true)) {
        motion.x = 0.0f;
        motion.z = 0.0f;
    }
    if (flagOr(options, "stop_vertical_movement", true))
        motion.y = 0.0f;
    mob.setMotion(motion);
}

void EntityEvents::_queueCommands(ServerNetworkHandler &owner, MobActor &mob, const json::Value &queue) {
    const json::Value *command = queue.get("command");
    if (command == nullptr)
        return;

    if (command->isString()) {
        owner.queueActorCommand(mob, command->mString);
        return;
    }

    for (const std::unique_ptr<json::Value> &entry: command->mArray) {
        if (entry->isString())
            owner.queueActorCommand(mob, entry->mString);
    }
}

bool EntityEvents::_run(ServerNetworkHandler &owner, MobActor &mob, const json::Value &node, int32_t depth,
                        Actor *other) {
    if (depth > MAX_DEPTH || !node.isObject())
        return false;

    if (const json::Value *filters = node.get("filters")) {
        if (!EntityFilter::test(*filters, owner, mob, other))
            return false;
    }

    if (const json::Value *remove = node.get("remove"))
        _applyGroups(mob, *remove, false);
    if (const json::Value *add = node.get("add"))
        _applyGroups(mob, *add, true);
    if (const json::Value *properties = node.get("set_property"))
        _setProperties(owner, mob, *properties);

    if (node.get("reset_target") != nullptr)
        mob.clearTarget();
    if (const json::Value *stop = node.get("stop_movement"))
        _stopMovement(mob, *stop);
    if (node.get("set_home_position") != nullptr)
        mob.setHomePosition(mob.getPosition());

    if (const json::Value *drop = node.get("drop_item")) {
        const json::Value *slot = drop->get("slot");
        if (slot != nullptr && mob.getEquipment().dropSlot(owner, owner.getLevelFor(mob), mob.getPosition(),
                                                            slot->string()))
            mob.getEquipment().broadcast(owner, mob);
    }

    if (const json::Value *sound = node.get("play_sound")) {
        const json::Value *name = sound->get("sound");
        if (name != nullptr)
            mob.playDefinitionSound(owner, name->string());
    }

    if (const json::Value *queue = node.get("queue_command"))
        _queueCommands(owner, mob, *queue);

    if (const json::Value *homeEvent = node.get("execute_event_on_home_block")) {
        const json::Value *name = homeEvent->get("event");
        if (name != nullptr && mob.hasHome())
            mob.fireBlockEvent(owner, blockAt(mob.getHomePosition()), name->string());
    }

    if (const json::Value *sequence = node.get("sequence")) {
        for (const std::unique_ptr<json::Value> &entry: sequence->mArray)
            _run(owner, mob, *entry, depth + 1, other);
    }

    if (const json::Value *firstValid = node.get("first_valid")) {
        for (const std::unique_ptr<json::Value> &entry: firstValid->mArray) {
            if (_run(owner, mob, *entry, depth + 1, other))
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
                    _run(owner, mob, *entry, depth + 1, other);
                    break;
                }
            }
        }
    }

    if (const json::Value *trigger = node.get("trigger"))
        fireTrigger(owner, mob, trigger, other, depth + 1);

    return true;
}
