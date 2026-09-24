#include "Block/Blocks/PlantGrowthHelpers.h"

#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

namespace PlantGrowthHelpers {
    const char *AGE = "age";
    const char *AGE_BIT = "age_bit";
    const char *LEAF_SIZE = "bamboo_leaf_size";
    const char *STALK_THICKNESS = "bamboo_stalk_thickness";

    namespace {
        const Vector3i FACE_OFFSETS[] = {
                Vector3i(0, -1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1),
                Vector3i(0, 0, 1), Vector3i(-1, 0, 0), Vector3i(1, 0, 0)
        };
    }

    Vector3i side(const Vector3i &position, int32_t face) {
        const Vector3i &offset = FACE_OFFSETS[face];
        return Vector3i(position.x + offset.x, position.y + offset.y, position.z + offset.z);
    }

    Vector3i above(const Vector3i &position) {
        return side(position, FACE_UP);
    }

    Vector3i below(const Vector3i &position) {
        return side(position, FACE_DOWN);
    }

    int32_t opposite(int32_t face) {
        return face ^ 1;
    }

    void resetAge(Level &level, const Vector3i &position, const BlockState &state) {
        level.setBlock(position, DecorationSupport::withState(state, AGE, 0), false);
    }
}
