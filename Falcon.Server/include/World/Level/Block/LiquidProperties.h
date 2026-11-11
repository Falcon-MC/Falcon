#pragma once

#include <cstdint>

class BlockState;

enum class LiquidMaterial : uint8_t { None, Water, Lava };

// what spreading water does to the block in its way (BlockType +186, sub_149C3F9B0)
enum class LiquidReaction : uint8_t {
    Broken,
    Popped,
    Blocking,
    // water goes to layer 1 and the block stays
    NoReaction
};

namespace LiquidFace {
    // Facing order: masks and the directions liquid code passes around
    constexpr int DOWN = 0;
    constexpr int UP = 1;
    constexpr int NORTH = 2;
    constexpr int SOUTH = 3;
    constexpr int WEST = 4;
    constexpr int EAST = 5;

    constexpr int opposite(int face) {
        return face ^ 1;
    }
}

struct LiquidProperties {
    LiquidMaterial material = LiquidMaterial::None;
    // flowing_water and flowing_lava: turned into the still block as soon as they are placed or ticked
    bool dynamic = false;
    bool air = false;
    bool bubbleColumn = false;
    uint8_t depth = 0;
    LiquidReaction reaction = LiquidReaction::Blocking;
    bool lavaBlocking = true;
    // Material::isSolid: two water sources make a third on top of it
    bool solid = true;
    bool containsWater = false;
    // faces liquid cannot cross (BlockType +185, stops_liquid_flowing_from_direction)
    uint8_t blockedFaces = 0;
    // the block class takes tick queue hooks (Block::usesBlockTicks)
    bool ticking = false;

    bool isLiquid() const {
        return material != LiquidMaterial::None;
    }

    // cached on the state
    static LiquidProperties of(const BlockState &state);
};
