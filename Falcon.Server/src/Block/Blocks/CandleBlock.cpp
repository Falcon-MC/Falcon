#include "Block/Blocks/CandleBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CandleBlock, 360);

#include "Block/BlockIdentifier.h"
#include "Block/Blocks/PlacementHelpers.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"

using namespace PlacementHelpers;

namespace {
    const int CANDLES_MAX = 3;
    const char *CANDLES = "candles";
}

bool CandleBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:candle" || BlockIdentifier::endsWith(identifier, "_candle");
}

PlacementMergeResult CandleBlock::mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                                 const Vector3f &clickPosition, Vector3i &position,
                                                 BlockState &state) const {
    (void) blockFace;
    (void) clickPosition;

    Vector3i candidate = clickedPosition;
    BlockState existing = stateAt(level, candidate);
    if (!matches(existing.mName)) {
        const Vector3i above = PlacementOrientation::relativePosition(clickedPosition, PlacementOrientation::FACE_UP);
        const BlockState aboveState = stateAt(level, above);
        if (matches(aboveState.mName)) {
            candidate = above;
            existing = aboveState;
        } else {
            candidate = position;
            existing = stateAt(level, position);
        }
    }

    if (existing.mName == getIdentifier()) {
        const int32_t candles = existing.mStates.getInt(CANDLES, 0);
        if (candles >= CANDLES_MAX)
            return PlacementMergeResult::Rejected;

        Tag states = existing.mStates;
        states.putInt(CANDLES, candles + 1);
        position = candidate;
        state = BlockState(existing.mName, states);
        return PlacementMergeResult::Merged;
    }

    if (matches(existing.mName))
        return PlacementMergeResult::Rejected;

    return PlacementMergeResult::None;
}
