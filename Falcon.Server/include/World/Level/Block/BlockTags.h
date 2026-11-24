#pragma once

#include <string>
#include <vector>

class BlockState;

// the tags the game gives each block state (BedrockData's block_tags.json)
namespace BlockTags {
    bool has(const BlockState &state, const std::string &tag);

    // sorted: getTags of Block and BlockPermutation
    std::vector<std::string> all(const BlockState &state);
}
