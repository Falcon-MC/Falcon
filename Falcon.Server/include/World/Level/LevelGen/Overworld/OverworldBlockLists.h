#pragma once

#include "World/Level/LevelGen/BlockStateSet.h"

// block lists the game's overworld passes share
namespace OverworldBlockLists {
    // sand hardening into sandstone (list at 0x1529B0DF8, filled from 0x14F1F6538)
    const BlockStateSet &sand();
}
