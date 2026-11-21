#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class CakeBlock final : public Block {
public:
    static constexpr int32_t MAX_BITES = 6;
    // the saturation the game gives a slice
    static constexpr int32_t SLICE_NUTRITION = 2;
    static constexpr float SLICE_SATURATION = 0.4f;

    explicit CakeBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    // a cake that can't stay is gone, dropping nothing
    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    static bool canEat(ServerNetworkHandler &owner, const ServerPlayer &player);

    static void eatSlice(ServerNetworkHandler &owner, ServerPlayer &player, Level &level, const Vector3i &position,
                         const BlockState &cake);

    // the candle cake a candle makes, empty for any other item
    static std::string candleCakeFor(const std::string &candleIdentifier);
};

class CandleCakeBlock final : public Block {
public:
    explicit CandleCakeBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;

    static std::string candleOf(const std::string &candleCakeIdentifier);
};
