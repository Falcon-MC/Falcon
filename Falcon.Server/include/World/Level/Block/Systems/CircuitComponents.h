#pragma once

#include "Core/Math/Vector3i.h"

#include <optional>

class Level;
class ServerNetworkHandler;

// the blocks the game's circuit system holds a component for
namespace CircuitComponents {
    // the strength Block.getRedstonePower reads from the component; empty without one
    std::optional<int> strengthAt(ServerNetworkHandler &owner, Level &level, const Vector3i &position);
}
