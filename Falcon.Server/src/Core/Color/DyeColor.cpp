#include "Core/Color/DyeColor.h"

#include <deque>
#include <unordered_map>

namespace {
    const char *const NAMES[DyeColor::COUNT] = {
            "white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
            "light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black",
    };

    std::string buildIdentifier(uint8_t color, const char *suffix) {
        return std::string("minecraft:") + NAMES[color] + suffix;
    }

    const std::string &cachedIdentifier(uint8_t color, const char *suffix) {
        static std::deque<std::string> storage;
        static std::unordered_map<std::string, const std::string *> index;

        const std::string key = std::string(suffix) + "/" + NAMES[color];
        const auto match = index.find(key);
        if (match != index.end())
            return *match->second;

        storage.push_back(buildIdentifier(color, suffix));
        const std::string &stored = storage.back();
        index.emplace(key, &stored);
        return stored;
    }
}

const char *DyeColor::name(uint8_t color) {
    return NAMES[clamp(color)];
}

uint8_t DyeColor::fromName(const std::string &name) {
    for (uint8_t color = 0; color < COUNT; color++) {
        if (name == NAMES[color])
            return color;
    }

    return INVALID;
}

const char *DyeColor::identifier(uint8_t color, const char *suffix) {
    return cachedIdentifier(clamp(color), suffix).c_str();
}

uint8_t DyeColor::fromIdentifier(const std::string &identifier, const char *suffix) {
    for (uint8_t color = 0; color < COUNT; color++) {
        if (identifier == cachedIdentifier(color, suffix))
            return color;
    }

    return INVALID;
}

uint8_t DyeColor::invert(uint8_t color) {
    return (uint8_t) ((~clamp(color)) & 0xF);
}

uint8_t DyeColor::clamp(uint8_t color) {
    return color < COUNT ? color : 0;
}
