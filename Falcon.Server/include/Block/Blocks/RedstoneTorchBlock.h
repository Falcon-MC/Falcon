#pragma once

#include "Block/Blocks/TorchBlock.h"

#include <string>

class RedstoneTorchBlock final : public TorchBlock {
public:
    explicit RedstoneTorchBlock(const Block &block) : TorchBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override;

    bool isLit() const;

    static BlockState getLitState(const BlockState &state);

    static BlockState getUnlitState(const BlockState &state);
};
