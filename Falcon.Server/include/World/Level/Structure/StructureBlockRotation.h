#pragma once

#include "World/Level/Block/BlockState.h"

// the block a structure places once turned and mirrored (sub_145E93850, called by sub_146986070)
namespace StructureBlockRotation {
    // rotation in quarter turns clockwise, mirror 0 none, 1 x (flips z), 2 z (flips x), 3 both
    BlockState apply(const BlockState &state, int rotation, int mirror);
}
