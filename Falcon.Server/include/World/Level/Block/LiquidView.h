#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/BlockNames.h"

class LiquidView {
public:
    explicit LiquidView(const BlockState &state) : mState(state) {}

    const std::string &identifier() const {
        return mState.mName;
    }

    bool isWater() const {
        return mState.mName == BlockNames::WATER || mState.mName == "minecraft:flowing_water";
    }

    bool isLava() const {
        return mState.mName == BlockNames::LAVA || mState.mName == "minecraft:flowing_lava";
    }

    bool isLiquid() const {
        return isWater() || isLava();
    }

    bool isBubbleColumn() const {
        return mState.mName == "minecraft:bubble_column";
    }

    int getDepth() const {
        return mState.mStates.getInt("liquid_depth", 0);
    }

    // falling liquid has bit 3 of liquid_depth set
    int getDecay() const {
        return getDepth() >= 8 ? 0 : getDepth();
    }

    bool isSource() const {
        return isLiquid() && getDepth() == 0;
    }

    bool isFalling() const {
        return isLiquid() && getDepth() >= 8;
    }

    bool isDragDown() const {
        return mState.mStates.getBool("drag_down", false);
    }

    float getFluidHeightPercent() const {
        if (isBubbleColumn())
            return 1.0f;
        return (float) (isFalling() ? 0 : getDecay() + 1) / 9.0f;
    }

private:
    const BlockState &mState;
};
