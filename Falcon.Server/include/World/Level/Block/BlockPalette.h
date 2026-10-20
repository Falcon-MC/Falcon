#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstddef>
#include <vector>

class BlockPalette {
public:
    const BlockState &get(std::size_t index) const;

    std::size_t add(const BlockState &state);

    std::size_t size() const noexcept { return mStates.size(); }

    void clear() noexcept { mStates.clear(); }

private:
    std::vector<BlockState> mStates;
};
