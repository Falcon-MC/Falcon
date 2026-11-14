#pragma once

#include <string>

// the isSolid flag the game gives each block type (BedrockData's block_solid.json)
// not Block's mSolid, which says whether a block obstructs movement
namespace BlockSolidity {
    bool isSolid(const std::string &identifier);
}
