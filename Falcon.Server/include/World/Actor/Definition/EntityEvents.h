#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <string>
#include <vector>

class Actor;
class MobActor;
class ServerNetworkHandler;

class EntityEvents {
public:
    static void fire(ServerNetworkHandler &owner, MobActor &mob, const std::string &event, int32_t depth = 0,
                     Actor *other = nullptr);

    static void fireTrigger(ServerNetworkHandler &owner, MobActor &mob, const json::Value *trigger,
                            Actor *other = nullptr, int32_t depth = 0);

    static void fireTriggers(ServerNetworkHandler &owner, MobActor &mob, const json::Value *triggers,
                             Actor *other = nullptr);

    // runs the upgrades past version; returns the new ActorVersion, the upgrade count of the definition
    static int32_t upgrade(ServerNetworkHandler &owner, MobActor &mob, int32_t version);

private:
    struct Modifiers {
        std::vector<std::string> mAdded;
        std::vector<std::string> mRemoved;
    };

    // the groups the running event changed, read by the dataDrivenEntityTrigger script event
    static inline Modifiers *sModifiers = nullptr;

    static bool _fire(ServerNetworkHandler &owner, MobActor &mob, const std::string &event, int32_t depth, Actor *other,
                      Modifiers &modifiers);

    static bool _run(ServerNetworkHandler &owner, MobActor &mob, const json::Value &node, int32_t depth, Actor *other);

    static MobActor *_resolveTarget(ServerNetworkHandler &owner, MobActor &mob, const std::string &target,
                                    Actor *other);

    static void _applyGroups(MobActor &mob, const json::Value &change, bool add);

    static void _setProperties(ServerNetworkHandler &owner, MobActor &mob, const json::Value &properties);

    static void _stopMovement(MobActor &mob, const json::Value &options);

    static void _queueCommands(ServerNetworkHandler &owner, MobActor &mob, const json::Value &queue);
};
