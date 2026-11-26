#include "World/Level/Block/MultifaceBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(MultifaceBlock, 329);

#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <optional>

namespace {
    const char *const STATE_FACES = "multi_face_direction_bits";

    // the block toward direction offers a full face; nullopt when its chunk is not loaded
    std::optional<bool> holdsToward(Level &level, const Vector3i &position, int direction) {
        const Vector3i support = PlacementOrientation::relativePosition(position, direction);
        const BlockState *state = level.peekBlockPtr(support.x, support.y, support.z);
        if (state == nullptr)
            return std::nullopt;
        return PlacementHelpers::hasSolidFace(*state, direction ^ 1);
    }
}

bool MultifaceBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::SCULK_VEIN || identifier == BlockNames::RESIN_CLUMP;
}

// the face the block clings to has to be a full face, like when a vein spreads
bool MultifaceBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace < 0 || blockFace > 5)
        return false;
    return holdsToward(level, position, blockFace ^ 1).value_or(false);
}

// the game strips each face whose support is gone, and the block once no face is left
void MultifaceBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                        const BlockState &state) const {
    (void) owner;

    const int bits = state.mStates.getInt(STATE_FACES, 0);
    int kept = 0;
    for (int face = 0; face < 6; ++face) {
        const int bit = PlacementOrientation::multiFaceBit(face);
        if ((bits & bit) == 0)
            continue;
        // an unloaded neighbour is unknown: keep the face
        if (holdsToward(level, position, face).value_or(true))
            kept |= bit;
    }

    if (kept == bits)
        return;
    if (kept == 0) {
        level.setBlock(position, VanillaBlocks::AIR().toBlockState(), true);
        return;
    }

    Tag states = state.mStates;
    states.putInt(STATE_FACES, kept);
    level.setBlock(position, BlockState(state.mName, states), true);
}
