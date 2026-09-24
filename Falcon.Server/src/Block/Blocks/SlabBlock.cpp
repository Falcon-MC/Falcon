#include "Block/Blocks/SlabBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SlabBlock, 350);

#include "Block/BlockIdentifier.h"
#include "Block/Blocks/PlacementHelpers.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"

using namespace PlacementHelpers;

namespace {
    const char *VERTICAL_HALF = "minecraft:vertical_half";
    const float SLAB_HEIGHT = 0.5f;
}

bool SlabBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, SLAB_SUFFIX) && identifier.find("double_") == std::string::npos;
}

bool SlabBlock::isTopSlab(const BlockState &state) {
    return state.mStates.getString(VERTICAL_HALF, "bottom") == "top";
}

std::string SlabBlock::getDoubleSlabIdentifier() const {
    if (BlockIdentifier::endsWith(getIdentifier(), COPPER_SLAB_SUFFIX))
        return replaceSuffix(getIdentifier(), COPPER_SLAB_SUFFIX, DOUBLE_COPPER_SLAB_SUFFIX);

    return replaceSuffix(getIdentifier(), SLAB_SUFFIX, DOUBLE_SLAB_SUFFIX);
}

bool SlabBlock::getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
    shape = isTopSlab(state) ? AxisAlignedBB(0.0f, SLAB_HEIGHT, 0.0f, 1.0f, 1.0f, 1.0f)
                             : AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, SLAB_HEIGHT, 1.0f);
    return true;
}

PlacementMergeResult SlabBlock::mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                               const Vector3f &clickPosition, Vector3i &position,
                                               BlockState &state) const {
    using namespace PlacementOrientation;

    const Block *doubleSlab = VanillaBlocks::fromIdentifier(getDoubleSlabIdentifier());
    const BlockState clicked = stateAt(level, clickedPosition);
    const bool clickedSameSlab = clicked.mName == getIdentifier();

    bool top = clickPosition.y > 0.5f;
    if (blockFace == FACE_DOWN) {
        if (clickedSameSlab && isTopSlab(clicked)) {
            if (doubleSlab == nullptr)
                return PlacementMergeResult::Rejected;

            position = clickedPosition;
            state = doubleSlab->toBlockState();
            return PlacementMergeResult::Merged;
        }
        top = true;
    } else if (blockFace == FACE_UP) {
        if (clickedSameSlab && !isTopSlab(clicked)) {
            if (doubleSlab == nullptr)
                return PlacementMergeResult::Rejected;

            position = clickedPosition;
            state = doubleSlab->toBlockState();
            return PlacementMergeResult::Merged;
        }
        top = false;
    }

    const BlockState existing = stateAt(level, position);
    if (existing.mName != getIdentifier() || isTopSlab(existing) == top)
        return PlacementMergeResult::None;

    if (doubleSlab == nullptr)
        return PlacementMergeResult::Rejected;

    state = doubleSlab->toBlockState();
    return PlacementMergeResult::Merged;
}
