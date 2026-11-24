#include "World/Level/Block/LiquidBlock.h"

#include "World/Level/Block/LavaBlock.h"
#include "World/Level/Block/WaterBlock.h"

#include <algorithm>

bool LiquidBlock::canBeReplaced(const BlockState &state) const {
    (void) state;

    return true;
}

PistonMoveReaction LiquidBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

bool LiquidBlock::isWater() const {
    return WaterBlock::matches(getIdentifier());
}

bool LiquidBlock::isLava() const {
    return LavaBlock::matches(getIdentifier());
}

bool LiquidBlock::isLiquid() const {
    return isWater() || isLava();
}

bool LiquidBlock::isSource() const {
    return isLiquid() && getStates().getInt("liquid_depth", 0) == 0;
}

// falling liquid has bit 3 of liquid_depth set
bool LiquidBlock::isFalling() const {
    return isLiquid() && getStates().getInt("liquid_depth", 0) >= 8;
}

bool LiquidBlock::isBubbleColumn() const {
    return getIdentifier() == "minecraft:bubble_column";
}

bool LiquidBlock::isDragDown() const {
    return getStates().getBool("drag_down", false);
}

int LiquidBlock::getDecay() const {
    const int depth = getStates().getInt("liquid_depth", 0);
    return depth >= 8 ? 0 : depth;
}

int32_t LiquidBlock::getTickDelay(DimensionType dimension) const {
    (void) dimension;
    return 5;
}

const char *LiquidBlock::getBucketFillSound() const {
    return isLava() ? "bucket_fill_lava" : "bucket_fill_water";
}

const char *LiquidBlock::getBucketEmptySound() const {
    return isLava() ? "bucket_empty_lava" : "bucket_empty_water";
}

float LiquidBlock::getFluidHeightPercent() const {
    if (isBubbleColumn())
        return 1.0f;
    return (float) (isFalling() ? 0 : getDecay() + 1) / 9.0f;
}

// the game keeps every placed liquid as the still block
BlockState LiquidBlock::makeState(int decay, bool falling) const {
    Tag states = Tag::ofCompound();
    states.putInt("liquid_depth", std::max(0, std::min(7, decay)) + (falling ? 8 : 0));
    return BlockState(isLava() ? "minecraft:lava" : "minecraft:water", states);
}
