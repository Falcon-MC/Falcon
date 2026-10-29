#include "World/Level/Block/BlockPalette.h"

#include <stdexcept>

const BlockState &BlockPalette::get(std::size_t index) const {
    if (index >= mStates.size())
        throw std::out_of_range("BlockPalette index out of range");
    return mStates[index];
}

std::size_t BlockPalette::add(const BlockState &state) {
    for (std::size_t index = 0; index < mStates.size(); ++index) {
        if (mStates[index] == state)
            return index;
    }
    mStates.push_back(state);
    return mStates.size() - 1;
}
