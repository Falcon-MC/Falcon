#pragma once

#include "World/Level/Block/Systems/RedstoneSystem.h"

class Level;
class ServerNetworkHandler;

// without a running server the fixture ticks the circuit itself: it evaluates every second tick and then hands
// each changed component, and the block holding it, its new strength
inline void settleCircuit(ServerNetworkHandler &owner, Level &level, int ticks = 4) {
    for (int tick = 0; tick < ticks; ++tick) {
        RedstoneSystem::tickCircuit(owner, level);
        RedstoneSystem::tick(owner, level);
    }
}
