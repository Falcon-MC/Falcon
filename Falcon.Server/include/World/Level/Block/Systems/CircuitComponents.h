#pragma once

#include "Core/Math/Vector3i.h"

#include <optional>

class Level;
class ServerNetworkHandler;

// the blocks the game's circuit system holds a component for (CircuitSystem::create callers, sub_143D35420)
namespace CircuitComponents {
    // the strength Block.getRedstonePower reads from the component (sub_14A752ED0); empty without one
    std::optional<int> strengthAt(ServerNetworkHandler &owner, Level &level, const Vector3i &position);
}
