#pragma once

#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <vector>

class Actor;
class MobActor;
class ServerNetworkHandler;

enum class PushMode : uint8_t {
    Default,
    // the matching pusher does not push at all
    None,
    // kicks the actor away from what touched it
    Ball
};

// a minecraft:pushable_by_entity preset
struct PushableByEntityPreset {
    PushMode mMode = PushMode::Default;
    float mMinDistance = 0.0f;
    float mMaxDistance = 0.0f;
    bool mRequireCollisionOverlap = true;
    float mPushScaleSelf = 1.0f;
    float mKickSpeedScale = 0.0f;
    float mMinKickSpeed = 0.0f;
    float mMaxKickSpeed = 0.0f;
    float mVerticalKickMultiplier = 0.0f;
    bool mPlaySound = false;
    float mSoundImpulseThreshold = 0.0f;
    int32_t mSoundCooldownTicks = 0;
    const json::Value *mFilters = nullptr;

    static PushableByEntityPreset read(const json::Value &preset);
};

// the presets of one component, parsed once per component change
struct PushableByEntityPresets {
    const json::Value *mComponent = nullptr;
    std::vector<PushableByEntityPreset> mPresets;
    // a ball preset reaching past the boxes: pushers this far away are still candidates
    float mReach = 0.0f;

    void refresh(const json::Value *component);
};

class PushableByEntity {
public:
    static constexpr const char *KICK_SOUND = "pushed_by_player";

    // the first preset whose filters accept the pusher; nullptr: the default push
    static const PushableByEntityPreset *findPreset(ServerNetworkHandler &owner, const MobActor &self,
                                                    const PushableByEntityPresets &presets, const Actor &pusher);

    // offset: pusher position minus self position, horizontal only; false when out of the preset's reach
    static bool inReach(const PushableByEntityPreset &preset, float offsetX, float offsetZ, float pusherWidth);

    // the velocity the kick adds to self; pusherSpeed is the pusher's speed last tick
    static Vector3f kick(const PushableByEntityPreset &preset, float offsetX, float offsetZ, float pusherSpeed,
                         bool selfOnGround);
};
