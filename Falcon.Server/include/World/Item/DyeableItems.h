#pragma once

#include <string>

namespace DyeableItems {
    // the leather tier armor, the horse armors, wolf armor and pack items with minecraft:dyeable take a color
    bool takesColor(const std::string &identifier);

    // customColor holds the color as ARGB
    constexpr const char *TAG_CUSTOM_COLOR = "customColor";
}
