#include "Block/Blocks/GrowthHelpers.h"

namespace GrowthHelpers {
    BlockState withState(const BlockState &state, const std::string &name, int32_t value) {
        Tag states = state.mStates;
        states.putInt(name, value);
        return BlockState(state.mName, states);
    }
}
