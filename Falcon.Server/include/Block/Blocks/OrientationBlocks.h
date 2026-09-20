#pragma once

#include "Block/Block.h"

#include <string>

class FacingMachineBlock : public Block {
public:
    explicit FacingMachineBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};

class PistonBlock final : public FacingMachineBlock {
public:
    explicit PistonBlock(const Block &block) : FacingMachineBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                  const BlockState &state) const override;
};

class TorchOrientationBlock final : public Block {
public:
    explicit TorchOrientationBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;
};

class WallAttachedBlock : public Block {
public:
    explicit WallAttachedBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};

class BellOrientationBlock final : public Block {
public:
    explicit BellOrientationBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};

class FaceAttachedBlock final : public Block {
public:
    explicit FaceAttachedBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};

class CardinalPlayerBlock : public Block {
public:
    explicit CardinalPlayerBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};

class BedOrientationBlock final : public CardinalPlayerBlock {
public:
    explicit BedOrientationBlock(const Block &block) : CardinalPlayerBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    std::vector<BlockPlacementEntry> getPlacementBlocks(Level &level, const Vector3i &position,
                                                        const BlockState &state,
                                                        int playerFacing) const override;

    void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                  const BlockState &state) const override;

    std::vector<Vector3i> getAffectedBlocks(Level &level, const Vector3i &position,
                                            const BlockState &state) const override;
};

class DoorOrientationBlock final : public Block {
public:
    explicit DoorOrientationBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    std::vector<BlockPlacementEntry> getPlacementBlocks(Level &level, const Vector3i &position,
                                                        const BlockState &state,
                                                        int playerFacing) const override;

    std::vector<Vector3i> getAffectedBlocks(Level &level, const Vector3i &position,
                                            const BlockState &state) const override;
};

class TrapdoorOrientationBlock final : public Block {
public:
    explicit TrapdoorOrientationBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};
