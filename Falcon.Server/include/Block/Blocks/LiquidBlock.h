#pragma once

#include "Block/Block.h"

class LiquidBlock : public Block {
public:
    LiquidBlock(int32_t typeId, const std::string &identifier, const std::string &name)
            : Block(typeId, identifier, name) {}

    explicit LiquidBlock(const BlockState &state) : Block(state) {}

    explicit LiquidBlock(const Block &block) : Block(block) {}

    bool canBeReplaced(const BlockState &state) const override;

    PistonMoveReaction getPistonMoveReaction() const override;

    bool isWater() const;
    bool isLava() const;
    bool isLiquid() const;
    bool isSource() const;
    bool isFalling() const;
    bool isStill() const;
    bool isBubbleColumn() const;
    bool isDragDown() const;
    int getDecay() const;
    virtual int getTickRate() const;
    virtual int getFlowDecayPerBlock() const;
    virtual int getMinAdjacentSourcesToFormSource() const;
    virtual const char *getBucketFillSound() const;
    virtual const char *getBucketEmptySound() const;
    float getFluidHeightPercent() const;
    BlockState makeState(int decay, bool falling = false) const;
};
