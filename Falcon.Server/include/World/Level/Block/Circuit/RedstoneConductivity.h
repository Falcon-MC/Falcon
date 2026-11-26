#pragma once

#include "World/Level/Block/BlockState.h"

// the game's minecraft:redstone_conductivity
struct RedstoneConductivity {
    bool mConductor = false;
    bool mAllowsWireToStepDown = true;

    // a vanilla block's, as the game sets it up
    static RedstoneConductivity of(const BlockState &state);
};
