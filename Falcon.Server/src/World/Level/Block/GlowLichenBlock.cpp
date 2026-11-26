#include "World/Level/Block/GlowLichenBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(GlowLichenBlock, 329);

#include "Network/Handler/BlockActionHandler.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <array>
#include <utility>
#include <vector>

namespace {
    const char *const STATE_FACES = "multi_face_direction_bits";

    // a source of water the lichen takes into its second layer
    bool isWaterSource(const BlockState &state) {
        return LiquidView(state).isWater() && state.mStates.getInt("liquid_depth", 0) == 0;
    }

    // lichen can grow at position on face: air, water or a lichen missing that face, with a full face behind
    bool canGrowAt(Level &level, const Vector3i &position, int face) {
        const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
        if (state == nullptr)
            return false;
        if (state->mName == BlockNames::GLOW_LICHEN) {
            if ((state->mStates.getInt(STATE_FACES, 0) & PlacementOrientation::multiFaceBit(face)) != 0)
                return false;
        } else if (state->mName != BlockNames::AIR && !isWaterSource(*state)) {
            return false;
        }

        const Vector3i support = PlacementOrientation::relativePosition(position, face);
        const BlockState *behind = level.peekBlockPtr(support.x, support.y, support.z);
        return behind != nullptr && PlacementHelpers::hasSolidFace(*behind, face ^ 1);
    }

    void grow(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face) {
        const BlockState previous = level.getBlockState(position.x, position.y, position.z);
        const bool lichen = previous.mName == BlockNames::GLOW_LICHEN;
        Tag states = lichen ? previous.mStates : VanillaBlocks::GLOW_LICHEN().toBlockState().mStates;
        states.putInt(STATE_FACES,
                      (lichen ? previous.mStates.getInt(STATE_FACES, 0) : 0) | PlacementOrientation::multiFaceBit(face));
        if (!BlockChangeSystem::change(level, position, BlockState(BlockNames::GLOW_LICHEN, states),
                                       BlockChangeCause::Grow, true))
            return;
        // the water it grew into stays around it
        if (isWaterSource(previous)) {
            level.setBlockStateAtLayer(position.x, position.y, position.z, 1, previous);
            BlockActionHandler::broadcastBlockUpdate(owner, level, position, previous, 1);
        }
    }

    template<typename T>
    void shuffle(std::vector<T> &values) {
        for (size_t index = values.size(); index > 1; --index)
            std::swap(values[index - 1], values[(size_t) RandomTickSystem::nextInt((int) index)]);
    }
}

bool GlowLichenBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::GLOW_LICHEN;
}

// one new face from a random face toward a random side: the same block first, then the neighbour on the same
// plane, then around the corner; nothing grows when every spot is taken and the bone meal is kept
bool GlowLichenBlock::onFertilized(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state) const {
    const int bits = state.mStates.getInt(STATE_FACES, 0);
    std::vector<int> faces;
    for (int face = 0; face < 6; ++face) {
        if ((bits & PlacementOrientation::multiFaceBit(face)) != 0)
            faces.push_back(face);
    }
    std::vector<int> directions = {0, 1, 2, 3, 4, 5};
    shuffle(faces);
    shuffle(directions);

    for (int face: faces) {
        for (int direction: directions) {
            // a side on the face's own axis is the face or its back
            if ((direction >> 1) == (face >> 1))
                continue;

            const Vector3i side = PlacementOrientation::relativePosition(position, direction);
            const std::array<std::pair<Vector3i, int>, 3> spots = {
                    std::make_pair(position, direction), std::make_pair(side, face),
                    std::make_pair(PlacementOrientation::relativePosition(side, face), direction ^ 1)};
            for (const auto &[spot, spotFace]: spots) {
                if (canGrowAt(level, spot, spotFace)) {
                    grow(owner, level, spot, spotFace);
                    return true;
                }
            }
        }
    }
    return false;
}
