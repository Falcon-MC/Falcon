#include "Block/Blocks/DoubleSlabBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(DoubleSlabBlock, 355);

#include "Block/BlockIdentifier.h"
#include "Block/Blocks/PlacementHelpers.h"

using namespace PlacementHelpers;

namespace {
    const int32_t DOUBLE_SLAB_RESOURCE_COUNT = 2;
}

bool DoubleSlabBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, DOUBLE_SLAB_SUFFIX)
           || BlockIdentifier::endsWith(identifier, DOUBLE_COPPER_SLAB_SUFFIX);
}

std::string DoubleSlabBlock::getSlabIdentifier() const {
    if (BlockIdentifier::endsWith(getIdentifier(), DOUBLE_COPPER_SLAB_SUFFIX))
        return replaceSuffix(getIdentifier(), DOUBLE_COPPER_SLAB_SUFFIX, COPPER_SLAB_SUFFIX);

    return replaceSuffix(getIdentifier(), DOUBLE_SLAB_SUFFIX, SLAB_SUFFIX);
}

std::string DoubleSlabBlock::getResourceItem(const BlockState &state) const {
    (void) state;
    return getSlabIdentifier();
}

int32_t DoubleSlabBlock::getResourceCount(const BlockState &state) const {
    (void) state;
    return DOUBLE_SLAB_RESOURCE_COUNT;
}
