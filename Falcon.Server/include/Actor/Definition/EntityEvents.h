#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <string>

class MobActor;
class ServerNetworkHandler;

class EntityEvents {
public:
    static void fire(ServerNetworkHandler &owner, MobActor &mob, const std::string &event, int32_t depth = 0);

private:
    static bool _run(ServerNetworkHandler &owner, MobActor &mob, const json::Value &node, int32_t depth);

    static void _applyGroups(MobActor &mob, const json::Value &change, bool add);

    static void _setProperties(ServerNetworkHandler &owner, MobActor &mob, const json::Value &properties);
};
