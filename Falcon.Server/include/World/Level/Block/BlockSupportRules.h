#pragma once

#include <optional>

class BlockState;
class Level;
struct Vector3i;

// the blocks that need the block under them, and the states that hold each, as the game answers them
// (BedrockData's block_support_rules.json)
namespace BlockSupportRules {
    // empty for a state the measure did not find fragile, its class decides; true when it stands on a state
    // that holds it or over an unloaded chunk
    std::optional<bool> survivesAt(Level &level, const Vector3i &position, const BlockState &state);
}
