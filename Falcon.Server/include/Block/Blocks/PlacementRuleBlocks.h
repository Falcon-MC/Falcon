#pragma once

#include "Block/Block.h"
#include "Block/Blocks/OrientationBlocks.h"

#include <string>

class ReplaceableBlock final : public Block {
public:
    explicit ReplaceableBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canBeReplaced(const BlockState &state) const override;
};

class CarpetBlock final : public Block {
public:
    explicit CarpetBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;
};

class PressurePlateBlock final : public Block {
public:
    explicit PressurePlateBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;
};

class RedstoneWireBlock final : public Block {
public:
    explicit RedstoneWireBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;
};

class LadderBlock final : public WallAttachedBlock {
public:
    explicit LadderBlock(const Block &block) : WallAttachedBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;
};
