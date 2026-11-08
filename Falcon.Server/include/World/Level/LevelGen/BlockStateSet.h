#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_set>

class Block;

// matched by state hash: one integer lookup in the generation loops instead of names; read-only once built
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
