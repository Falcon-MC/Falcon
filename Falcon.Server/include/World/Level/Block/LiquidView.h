#pragma once

#include "World/Level/Block/BlockState.h"

class LiquidView {
public:
    explicit LiquidView(const BlockState &state) : mState(state) {}

    const std::string &identifier() const { return mState.mName; }

    bool isWater() const {
        return mState.mName == "minecraft:water" || mState.mName == "minecraft:flowing_water";
    }

    bool isLava() const {
        return mState.mName == "minecraft:lava" || mState.mName == "minecraft:flowing_lava";
    }

    bool isLiquid() const { return isWater() || isLava(); }

    bool isBubbleColumn() const { return mState.mName == "minecraft:bubble_column"; }

    int getDecay() const { return mState.mStates.getInt("liquid_depth", 0); }

    bool isSource() const {
        return isLiquid() && mState.mName != "minecraft:flowing_water"
               && mState.mName != "minecraft:flowing_lava" && getDecay() == 0;
    }

    bool isFalling() const {
        return isLiquid() && mState.mName != "minecraft:water" && mState.mName != "minecraft:lava"
               && getDecay() == 0;
    }

    bool isDragDown() const { return mState.mStates.getBool("drag_down", false); }

    int getFlowDecayPerBlock() const { return isLava() ? 2 : 1; }

    int getTickRate() const { return isLava() ? 30 : 5; }

    int getMinAdjacentSourcesToFormSource() const { return isWater() ? 2 : 0; }

    float getFluidHeightPercent() const {
        if (isBubbleColumn())
            return 1.0f;
        return (float) (isFalling() ? 0 : getDecay() + 1) / 9.0f;
    }

private:
    const BlockState &mState;
};
