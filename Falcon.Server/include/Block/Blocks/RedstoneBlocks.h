#pragma once

#include "Block/Block.h"
#include "Block/Blocks/ChestBlock.h"
#include "Block/Blocks/OrientationBlocks.h"

#include <string>

class RedstoneBlock final : public Block {
public:
    explicit RedstoneBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override;
};

class RedstoneLampBlock final : public Block {
public:
    explicit RedstoneLampBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isLit() const;

    static BlockState getLitState(const BlockState &state);

    static BlockState getUnlitState(const BlockState &state);
};

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

class ObserverBlock final : public FacingMachineBlock {
public:
    explicit ObserverBlock(const Block &block) : FacingMachineBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override;
};

class TrappedChestBlock final : public ChestBlock {
public:
    explicit TrappedChestBlock(const Block &block) : ChestBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override;
};
