#pragma once

#include <string>

class BlockState;

// the tags the game gives each block state (BedrockData's block_tags.json)
namespace BlockTags {
    bool has(const BlockState &state, const std::string &tag);
}
