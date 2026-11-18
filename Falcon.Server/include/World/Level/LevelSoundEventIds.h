#pragma once

#include <cstdint>
#include <string>

// the game's LevelSoundEvent numbers: components keep a sound as one of them
namespace LevelSoundEventIds {
    // no sound, and what any unknown name becomes
    constexpr int32_t UNDEFINED = 615;

    // the event a name reflects, compared lowercased; UNDEFINED for any other name
    int32_t fromName(const std::string &name);

    // the reflected name of an event, empty for UNDEFINED or an unknown number
    const char *nameOf(int32_t id);
}
