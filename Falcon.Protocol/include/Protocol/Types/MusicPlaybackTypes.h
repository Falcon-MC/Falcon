#pragma once

#include <string>

namespace MusicPlaybackState {
    inline constexpr const char *LOCAL = "Local";
    inline constexpr const char *ONLINE = "Online";
    inline constexpr const char *NOT_FOUND = "NotFound";
    inline constexpr const char *PENDING = "Pending";
}

class MusicPlaybackFailureEntry {
public:
    std::string mState;
    std::string mDescription;
};
