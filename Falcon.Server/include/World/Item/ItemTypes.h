#pragma once

#include <cstdint>

enum class ToolType : uint8_t {
    None = 0,
    Sword = 1,
    Shovel = 2,
    Pickaxe = 3,
    Axe = 4,
    Shears = 5,
    Hoe = 6
};

enum class ToolTier : int32_t {
    None = 0,
    Wooden = 1,
    Gold = 2,
    Stone = 3,
    Copper = 4,
    Iron = 5,
    Diamond = 6,
    Netherite = 7
};

enum class ArmorTier : int32_t {
    None = 0,
    Leather = 1,
    Iron = 2,
    Chain = 3,
    Copper = 4,
    Gold = 5,
    Diamond = 6,
    Netherite = 7
};

enum class ArmorSlot : uint8_t {
    None = 0,
    Head = 1,
    Chest = 2,
    Legs = 3,
    Feet = 4
};
