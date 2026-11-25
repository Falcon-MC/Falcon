#pragma once

#include <cstdint>

enum class SoundDataUpdateType : uint8_t {
    Stop = 0,
    SetVolume = 1,
    SetPitch = 2,
    Fade = 3,
    SeekTo = 4,
    Pause = 5,
    Resume = 6
};

class FadeSoundData {
public:
    float mTargetVolume = 0.0f;
    float mDuration = 0.0f;
};

class SetVolumeSoundData {
public:
    float mVolume = 0.0f;
};

class SetPitchSoundData {
public:
    float mPitch = 0.0f;
};

class SeekToSoundData {
public:
    float mSeconds = 0.0f;
};
