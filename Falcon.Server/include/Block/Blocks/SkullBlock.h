#pragma once

#include "Block/Blocks/FaceAttachedBlock.h"

#include <string>

class SkullBlock final : public FaceAttachedBlock {
public:
    explicit SkullBlock(const Block &block) : FaceAttachedBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    PistonMoveReaction getPistonMoveReaction() const override;
};
