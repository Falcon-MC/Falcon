#pragma once

#include "Block/Block.h"
#include "Block/BlockQuery.h"

#include <string>

namespace PlacementHelpers {
    using BlockQuery::stateAt;

    extern const std::string SLAB_SUFFIX;
    extern const std::string DOUBLE_SLAB_SUFFIX;
    extern const std::string COPPER_SLAB_SUFFIX;
    extern const std::string DOUBLE_COPPER_SLAB_SUFFIX;

    BlockState belowOf(Level &level, const Vector3i &position);

    std::string replaceSuffix(const std::string &identifier, const std::string &suffix,
                              const std::string &replacement);
}
