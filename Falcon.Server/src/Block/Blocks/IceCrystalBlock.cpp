#include "Block/Blocks/IceCrystalBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Level/Level.h"

#include <string>

FALCON_REGISTER_BLOCK(IceCrystalBlock, 185);

namespace {
    const char *BLOCK_FACE = "minecraft:block_face";

    Vector3i supportOffset(const std::string &face) {
        if (face == "down")
            return Vector3i(0, 1, 0);
        if (face == "north")
            return Vector3i(0, 0, 1);
        if (face == "south")
            return Vector3i(0, 0, -1);
        if (face == "west")
            return Vector3i(1, 0, 0);
        if (face == "east")
            return Vector3i(-1, 0, 0);
        return Vector3i(0, -1, 0);
    }
}

bool IceCrystalBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:ice_crystal";
}

bool IceCrystalBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    const Vector3i offset = supportOffset(state.mStates.getString(BLOCK_FACE, "up"));
    return level.isSolidAt(position.x + offset.x, position.y + offset.y, position.z + offset.z);
}
