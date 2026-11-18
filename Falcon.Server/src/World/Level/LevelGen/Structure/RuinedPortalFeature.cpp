#include "World/Level/LevelGen/Structure/RuinedPortalFeature.h"

namespace {
    const int32_t SPACING = 40;
    const int32_t SEPARATION = 15;
    const int32_t SALT = 40552231;

    const RuinedPortalFeature gRuinedPortal;
}

RuinedPortalFeature::RuinedPortalFeature()
        : StructureFeature("ruined_portal", SPACING, SEPARATION, SALT, Spread::Linear) {}

// every candidate starts a portal: the biome only shapes the piece
bool RuinedPortalFeature::_isValidStart(const ChunkGenerator &, int32_t, int32_t) const {
    return true;
}
