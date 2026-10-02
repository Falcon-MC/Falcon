#pragma once

#include "Core/Json/Json.h"
#include "Core/Math/Vector3i.h"

#include <string>

class Actor;
class BlockState;
class Level;
class MobActor;
class ServerNetworkHandler;

class BlockSensor {
public:
    static void onBlockBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state, Actor &source);

private:
    static void _sense(ServerNetworkHandler &owner, MobActor &mob, const json::Value &sensor,
                       const Vector3i &position, const std::string &block, Actor &source);

    static bool _matchesSource(ServerNetworkHandler &owner, MobActor &mob, const json::Value &sensor, Actor &source);
};
