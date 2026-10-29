#include "World/Level/Block/PlacementHelpers.h"

#include "World/Level/Level.h"
#include "World/Level/Block/BlockShape.h"

namespace PlacementHelpers {
    bool hasSolidFace(const BlockState &state, int face) {
        if (!BlockShape::hasCollision(state))
            return false;
        const AxisAlignedBB shape = BlockShape::getRelativeShape(state);
        const bool fullX = shape.mMinX <= 0.0f && shape.mMaxX >= 1.0f;
        const bool fullY = shape.mMinY <= 0.0f && shape.mMaxY >= 1.0f;
        const bool fullZ = shape.mMinZ <= 0.0f && shape.mMaxZ >= 1.0f;
        switch (face) {
            case 0: return fullX && fullZ && shape.mMinY <= 0.0f;
            case 1: return fullX && fullZ && shape.mMaxY >= 1.0f;
            case 2: return fullX && fullY && shape.mMinZ <= 0.0f;
            case 3: return fullX && fullY && shape.mMaxZ >= 1.0f;
            case 4: return fullY && fullZ && shape.mMinX <= 0.0f;
            case 5: return fullY && fullZ && shape.mMaxX >= 1.0f;
            default: return false;
        }
    }

    const std::string SLAB_SUFFIX = "_slab";
    const std::string DOUBLE_SLAB_SUFFIX = "_double_slab";
    const std::string COPPER_SLAB_SUFFIX = "cut_copper_slab";
    const std::string DOUBLE_COPPER_SLAB_SUFFIX = "double_cut_copper_slab";

    BlockState belowOf(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y - 1, position.z);
    }

    std::string replaceSuffix(const std::string &identifier, const std::string &suffix,
                              const std::string &replacement) {
        return identifier.substr(0, identifier.size() - suffix.size()) + replacement;
    }
}
