#pragma once

#include "Block/Blocks/PlacementRuleBlocks.h"
#include "Block/Blocks/PlantBlock.h"

class CactusBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};

class ReedsBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};

class BambooSaplingBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;
};

class BambooBlock : public Block {
public:
    explicit BambooBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    static int32_t countStalkBelow(Level &level, const Vector3i &position);

    static void grow(Level &level, const Vector3i &position, const BlockState &state, int32_t height);
};

class KelpBlock : public Block {
public:
    explicit KelpBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};

class VineBlock : public ReplaceableBlock {
public:
    explicit VineBlock(const Block &block) : ReplaceableBlock(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    static bool canSpread(Level &level, const Vector3i &position);

    static void putVine(Level &level, const Vector3i &position, int32_t bits);

    static void putVineOnHorizontalFace(Level &level, const Vector3i &position, int32_t bits);
};

class SweetBerryBushBlock : public Block {
public:
    explicit SweetBerryBushBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};

class CocoaBlock : public Block {
public:
    explicit CocoaBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};

class ChorusFlowerBlock : public Block {
public:
    explicit ChorusFlowerBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    static bool allNeighboursEmpty(Level &level, const Vector3i &position, int32_t exceptFace);

    static void placeFlower(Level &level, const Vector3i &position, int32_t age);
};

class CaveVinesBlock : public Block {
public:
    explicit CaveVinesBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
