#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>

class Level;
class ServerNetworkHandler;

namespace PortalHelpers {
    inline const char *AIR_IDENTIFIER = "minecraft:air";
    inline const char *OBSIDIAN_IDENTIFIER = "minecraft:obsidian";
    inline const char *PORTAL_IDENTIFIER = "minecraft:portal";
    inline const char *END_PORTAL_IDENTIFIER = "minecraft:end_portal";
    inline const char *END_PORTAL_FRAME_IDENTIFIER = "minecraft:end_portal_frame";

    Vector3f centerOf(const Vector3i &position);

    bool isInsideLevel(Level &level, const Vector3i &position);

    std::string identifierAt(Level &level, int32_t x, int32_t y, int32_t z);

    bool isAirAt(Level &level, int32_t x, int32_t y, int32_t z);

    std::string stateText(const BlockState &state, const std::string &key);

    BlockState makePortalState(const char *axis);

    void writeBlock(Level &level, const Vector3i &position, const BlockState &state, ServerNetworkHandler *owner);
}
