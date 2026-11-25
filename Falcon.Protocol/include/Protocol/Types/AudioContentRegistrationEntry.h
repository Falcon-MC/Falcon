#pragma once

#include <string>

namespace AudioContentPlaybackType {
    inline constexpr const char *MUSIC = "Music";
    inline constexpr const char *SOUND = "Sound";
    inline constexpr const char *CONCURRENT_MUSIC = "ConcurrentMusic";
}

class AudioContentRegistrationEntry {
public:
    std::string mAudioContentId;
    std::string mSharedMetadata;
    std::string mServerContent;
    std::string mPlaybackContent;
};
