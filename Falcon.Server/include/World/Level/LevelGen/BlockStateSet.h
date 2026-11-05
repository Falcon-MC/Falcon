#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_set>

class Block;

/**
 * Every state of a fixed group of blocks, matched by state hash: the generation loops test a block with one integer
 * lookup instead of comparing names. Build the sets once, they are read-only afterwards and safe across threads.
 */
class BlockStateSet {
public:
    BlockStateSet(std::initializer_list<std::string> identifiers);

    static BlockStateSet ofBlocks(const std::function<bool(const Block &)> &matches);

    bool contains(const BlockState &state) const {
        return mHashes.count(state.getHash()) != 0;
    }

private:
    BlockStateSet() = default;

    void _add(const std::string &identifier);

    std::unordered_set<int32_t> mHashes;
};
