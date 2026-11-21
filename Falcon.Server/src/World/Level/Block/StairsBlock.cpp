#include "World/Level/Block/StairsBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/LiquidProperties.h"

FALCON_REGISTER_BLOCK(StairsBlock, 351);

bool StairsBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_stairs");
}

// the full half and the back: weirdo_direction 0 to 3 face east, west, south, north
uint8_t StairsBlock::getLiquidBlockedFaces(const BlockState &state) const {
    static const int BACK[4] = {LiquidFace::EAST, LiquidFace::WEST, LiquidFace::SOUTH, LiquidFace::NORTH};
    const int half = state.mStates.getBool("upside_down_bit") ? LiquidFace::UP : LiquidFace::DOWN;
    return (uint8_t) (1 << half | 1 << BACK[state.mStates.getInt("weirdo_direction") & 3]);
}
