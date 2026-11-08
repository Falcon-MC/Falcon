#pragma once

#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

class Actor;
class MobActor;
class ServerNetworkHandler;

// a minecraft:pushable_by_entity preset; push_mode ball kicks the actor away from what touched it
struct PushableByEntityPreset {
    bool mBall = false;
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

    static PushableByEntityPreset read(const json::Value &preset);
};

class PushableByEntity {
public:
    static constexpr const char *KICK_SOUND = "pushed_by_player";

    // the first preset whose filters accept the pusher; nullptr: the default push
    static const json::Value *findPreset(ServerNetworkHandler &owner, const MobActor &self, const Actor &pusher);

    // offset: pusher position minus self position, horizontal only; false when out of the preset's reach
    static bool inReach(const PushableByEntityPreset &preset, float offsetX, float offsetZ, float pusherWidth);

    // the velocity the kick adds to self; pusherSpeed is the pusher's speed last tick
    static Vector3f kick(const PushableByEntityPreset &preset, float offsetX, float offsetZ, float pusherSpeed,
                         bool selfOnGround);
};
