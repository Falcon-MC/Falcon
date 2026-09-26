#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <string>

class Actor;
class MobActor;
class ServerNetworkHandler;

class EntityEvents {
public:
    static void fire(ServerNetworkHandler &owner, MobActor &mob, const std::string &event, int32_t depth = 0,
                     Actor *other = nullptr);

    static void fireTrigger(ServerNetworkHandler &owner, MobActor &mob, const json::Value *trigger,
                            Actor *other = nullptr, int32_t depth = 0);

private:
    static bool _run(ServerNetworkHandler &owner, MobActor &mob, const json::Value &node, int32_t depth,
                     Actor *other);

    static MobActor *_resolveTarget(ServerNetworkHandler &owner, MobActor &mob, const std::string &target,
                                    Actor *other);

    static void _applyGroups(MobActor &mob, const json::Value &change, bool add);

    static void _setProperties(ServerNetworkHandler &owner, MobActor &mob, const json::Value &properties);

    static void _stopMovement(MobActor &mob, const json::Value &options);

    static void _queueCommands(ServerNetworkHandler &owner, MobActor &mob, const json::Value &queue);
};
