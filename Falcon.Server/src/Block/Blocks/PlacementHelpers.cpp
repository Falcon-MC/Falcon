#include "Block/Blocks/PlacementHelpers.h"

#include "Level/Level.h"

namespace PlacementHelpers {
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
