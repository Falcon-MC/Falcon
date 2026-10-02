#include "Block/BlockQuery.h"

#include "Level/Level.h"

namespace BlockQuery {
    BlockState stateAt(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z);
    }

    bool isAirAt(Level &level, const Vector3i &position) {
        return stateAt(level, position).mName == "minecraft:air";
    }

    bool isInRange(Level &level, const Vector3i &position) {
        return position.y >= level.getMinY() && position.y <= level.getMaxY();
    }
}
