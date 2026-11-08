#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <unordered_map>

class Actor;
class MobActor;
class ServerNetworkHandler;

// minecraft:area_attack: hurts what the entity filter accepts inside the box grown by damage_range
class MobAreaAttack {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

private:
    void _attack(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component, Actor &target);

    std::unordered_map<int64_t, int64_t> mCooldowns;
};
