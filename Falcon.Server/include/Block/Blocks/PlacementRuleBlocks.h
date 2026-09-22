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

class SnowLayerBlock final : public Block {
public:
    explicit SnowLayerBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canBeReplaced(const BlockState &state) const override;

    PlacementMergeResult mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                        const Vector3f &clickPosition, Vector3i &position,
                                        BlockState &state) const override;
};

class SlabBlock final : public Block {
public:
    explicit SlabBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    static bool isTopSlab(const BlockState &state);

    std::string getDoubleSlabIdentifier() const;

    PlacementMergeResult mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                        const Vector3f &clickPosition, Vector3i &position,
                                        BlockState &state) const override;

    bool getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const override;
};

class DoubleSlabBlock final : public Block {
public:
    explicit DoubleSlabBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    std::string getSlabIdentifier() const;

    std::string getResourceItem(const BlockState &state) const override;

    int32_t getResourceCount(const BlockState &state) const override;
};

class CandleBlock final : public Block {
public:
    explicit CandleBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    PlacementMergeResult mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                        const Vector3f &clickPosition, Vector3i &position,
                                        BlockState &state) const override;
};

class ScaffoldingBlock final : public Block {
public:
    explicit ScaffoldingBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    Vector3i resolvePlacementPosition(Level &level, const Vector3i &position, int blockFace) const override;

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    void onPlacing(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                   BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;
};

class CarpetBlock final : public Block {
public:
    explicit CarpetBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;
};

class PressurePlateBlock final : public Block {
public:
    explicit PressurePlateBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool isSignalSource() const override;

    int getSignalForEntityCount(int count) const;
};

class RedStoneWireBlock final : public Block {
public:
    explicit RedStoneWireBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;
};

class ShelfMushroomBlock final : public Block {
public:
    explicit ShelfMushroomBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    BlockState applyPlacementOrientation(const BlockState &state,
                                         const BlockPlacementContext &context) const override;
};

class LadderBlock final : public WallAttachedBlock {
public:
    explicit LadderBlock(const Block &block) : WallAttachedBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;
};
