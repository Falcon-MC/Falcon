#include "Item/Items/FireStarterHelpers.h"

#include "Block/BlockSupport.h"
#include "Block/Systems/FireSystem.h"
#include "Level/Level.h"

namespace FireStarterHelpers {
    Vector3i relativeToFace(const Vector3i &position, int32_t face) {
        switch (face) {
            case 0:
                return Vector3i(position.x, position.y - 1, position.z);
            case 1:
                return Vector3i(position.x, position.y + 1, position.z);
            case 2:
                return Vector3i(position.x, position.y, position.z - 1);
            case 3:
                return Vector3i(position.x, position.y, position.z + 1);
            case 4:
                return Vector3i(position.x - 1, position.y, position.z);
            default:
                return Vector3i(position.x + 1, position.y, position.z);
        }
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool isObsidian(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z).mName == "minecraft:obsidian";
    }

    bool canIgniteAgainst(Level &level, const Vector3i &target, const Vector3i &placement) {
        if (level.getBlockState(placement.x, placement.y, placement.z).mName != "minecraft:air")
            return false;

        const BlockState state = level.getBlockState(target.x, target.y, target.z);
        const int burnChance = FireSystem::getBurnChance(state.mName);

        return burnChance != FireSystem::UNBURNABLE && (BlockSupport::isSolid(state) || burnChance > 0);
    }
}
