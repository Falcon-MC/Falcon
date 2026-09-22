#include "Block/Blocks/LiquidBlock.h"

#include "Block/Blocks/LavaBlock.h"
#include "Block/Blocks/WaterBlock.h"

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
    return isLiquid() && getIdentifier() != "minecraft:flowing_water" && getIdentifier() != "minecraft:flowing_lava"
           && getDecay() == 0;
}

bool LiquidBlock::isFalling() const {
    return isLiquid() && getIdentifier() != "minecraft:water" && getIdentifier() != "minecraft:lava" && getDecay() == 0;
}

bool LiquidBlock::isStill() const {
    return getIdentifier() == "minecraft:water" || getIdentifier() == "minecraft:lava";
}

bool LiquidBlock::isBubbleColumn() const {
    return getIdentifier() == "minecraft:bubble_column";
}

bool LiquidBlock::isDragDown() const {
    return getStates().getBool("drag_down", false);
}

int LiquidBlock::getDecay() const {
    return getStates().getInt("liquid_depth", 0);
}

int LiquidBlock::getFlowDecayPerBlock() const {
    return isLava() ? 2 : 1;
}

int LiquidBlock::getTickRate() const {
    return isLava() ? 30 : 5;
}

int LiquidBlock::getMinAdjacentSourcesToFormSource() const {
    return isWater() ? 2 : 0;
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

BlockState LiquidBlock::makeState(int decay, bool falling) const {
    const std::string flowing = isLava() ? "minecraft:flowing_lava" : "minecraft:flowing_water";
    const std::string still = isLava() ? "minecraft:lava" : "minecraft:water";
    Tag states = Tag::ofCompound();
    states.putInt("liquid_depth", std::max(0, std::min(7, decay)));
    return BlockState(falling || decay > 0 ? flowing : still, states);
}
