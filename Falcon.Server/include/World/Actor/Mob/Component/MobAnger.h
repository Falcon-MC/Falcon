#pragma once

#include "Core/Json/Json.h"

#include <cstdint>

class Actor;
class MobActor;
class ServerNetworkHandler;

class MobAnger {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

    void onHurt(ServerNetworkHandler &owner, MobActor &mob, Actor *attacker);

    void onAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &victim);

private:
    void _start(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component);

    void _calm(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component);

    void _broadcast(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component, const Actor &target);

    void _resetDuration(const json::Value &component);

    void _resetSound(const json::Value &component);

    const json::Value *mComponent = nullptr;
    int32_t mTicks = 0;
    int32_t mSoundTicks = 0;
};
