#pragma once

#include <string>

namespace AudioContentPlaybackType {
    inline constexpr const char *MUSIC = "Music";
    inline constexpr const char *SOUND = "Sound";
}

class AudioContentRegistrationEntry {
public:
    std::string mAudioContentId;
    std::string mSharedMetadata;
    std::string mServerContent;
    std::string mPlaybackContent;
};
