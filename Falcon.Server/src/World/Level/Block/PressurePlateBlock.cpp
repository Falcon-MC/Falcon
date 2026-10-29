#include "World/Level/Block/PressurePlateBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(PressurePlateBlock, 390);

#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/FenceBlock.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>

using namespace PlacementHelpers;

namespace {
    const char *LIGHT_WEIGHTED_PRESSURE_PLATE = "minecraft:light_weighted_pressure_plate";
    const char *HEAVY_WEIGHTED_PRESSURE_PLATE = "minecraft:heavy_weighted_pressure_plate";
    const int LIGHT_WEIGHTED_MAX_WEIGHT = 15;
    const int HEAVY_WEIGHTED_MAX_WEIGHT = 150;
}

bool PressurePlateBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_pressure_plate");
}

bool PressurePlateBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    const BlockState below = belowOf(level, position);
    return BlockSupport::isAttachable(below, PlacementOrientation::FACE_UP)
           || VanillaBlocks::getAs<FenceBlock>(below.mName) != nullptr;
}

bool PressurePlateBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
}

bool PressurePlateBlock::isSignalSource() const {
    return true;
}

int PressurePlateBlock::getSignalForEntityCount(int count) const {
    if (getIdentifier() == LIGHT_WEIGHTED_PRESSURE_PLATE) {
        const int weight = std::min(count, LIGHT_WEIGHTED_MAX_WEIGHT);
        const float ratio = (float) weight / (float) LIGHT_WEIGHTED_MAX_WEIGHT;
        return (int) std::ceil(ratio * 15.0f);
    }

    if (getIdentifier() == HEAVY_WEIGHTED_PRESSURE_PLATE) {
        const int weight = std::min(count, HEAVY_WEIGHTED_MAX_WEIGHT);
        const float ratio = (float) weight / (float) HEAVY_WEIGHTED_MAX_WEIGHT;
        return std::max(1, (int) std::ceil(ratio * 15.0f));
    }

    return RedstoneSystem::MAX_SIGNAL;
}
