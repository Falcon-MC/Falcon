#pragma once

#include "Block/Blocks/TorchOrientationBlock.h"

#include <string>

class RedstoneTorchBlock final : public TorchOrientationBlock {
public:
    explicit RedstoneTorchBlock(const Block &block) : TorchOrientationBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override;

    bool isLit() const;

    static BlockState getLitState(const BlockState &state);

    static BlockState getUnlitState(const BlockState &state);
};
