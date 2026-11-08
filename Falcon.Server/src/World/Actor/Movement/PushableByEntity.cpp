#include "World/Actor/Movement/PushableByEntity.h"

#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"

#include <algorithm>
#include <cmath>

namespace {
    const char *const PUSHABLE_COMPONENT = "minecraft:pushable_by_entity";
    const float MIN_DIRECTION_LENGTH = 0.0001f;
    const float TICKS_PER_SECOND = 20.0f;

    float numberIn(const json::Value &preset, const char *key, float fallback) {
        const json::Value *value = preset.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    bool flagIn(const json::Value &preset, const char *key, bool fallback) {
        const json::Value *value = preset.get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }
}

PushableByEntityPreset PushableByEntityPreset::read(const json::Value &preset) {
    PushableByEntityPreset result;
    const json::Value *mode = preset.get("push_mode");
    result.mBall = mode != nullptr && mode->isString() && mode->mString == "ball";
    result.mMinDistance = numberIn(preset, "min_distance", result.mMinDistance);
    result.mMaxDistance = numberIn(preset, "max_distance", result.mMaxDistance);
    result.mRequireCollisionOverlap = flagIn(preset, "require_collision_overlap", result.mRequireCollisionOverlap);
    result.mPushScaleSelf = numberIn(preset, "push_scale_self", result.mPushScaleSelf);
    result.mKickSpeedScale = numberIn(preset, "kick_speed_scale", result.mKickSpeedScale);
    result.mMinKickSpeed = numberIn(preset, "min_kick_speed", result.mMinKickSpeed);
    result.mMaxKickSpeed = numberIn(preset, "max_kick_speed", result.mMaxKickSpeed);
    result.mVerticalKickMultiplier = numberIn(preset, "vertical_kick_multiplier", result.mVerticalKickMultiplier);
    result.mPlaySound = flagIn(preset, "play_sound", result.mPlaySound);
    result.mSoundImpulseThreshold = numberIn(preset, "play_sound_impulse_threshold", result.mSoundImpulseThreshold);
    result.mSoundCooldownTicks = (int32_t) (numberIn(preset, "play_sound_cooldown_in_seconds", 0.0f)
                                            * TICKS_PER_SECOND);
    return result;
}

const json::Value *PushableByEntity::findPreset(ServerNetworkHandler &owner, const MobActor &self, const Actor &pusher) {
    // sub_143A0B900
    const json::Value *component = self.getComponent(PUSHABLE_COMPONENT);
    const json::Value *presets = component == nullptr ? nullptr : component->get("presets");
    if (presets == nullptr || !presets->isArray())
        return nullptr;

    for (const std::unique_ptr<json::Value> &preset: presets->mArray) {
        const json::Value *filters = preset->get("filters");
        if (filters == nullptr || EntityFilter::test(*filters, owner, self, &pusher))
            return preset.get();
    }
    return nullptr;
}

bool PushableByEntity::inReach(const PushableByEntityPreset &preset, float offsetX, float offsetZ,
                               float pusherWidth) {
    // sub_143A0AF90
    const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    return preset.mMinDistance <= distance && distance - pusherWidth * 0.5f <= preset.mMaxDistance;
}

Vector3f PushableByEntity::kick(const PushableByEntityPreset &preset, float offsetX, float offsetZ, float pusherSpeed,
                                bool selfOnGround) {
    // sub_143A0C040
    float speed = pusherSpeed * preset.mKickSpeedScale;
    speed = speed > preset.mMaxKickSpeed ? preset.mMaxKickSpeed : std::max(speed, preset.mMinKickSpeed);

    const float length = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    const float directionX = length < MIN_DIRECTION_LENGTH ? 0.0f : offsetX / length;
    const float directionZ = length < MIN_DIRECTION_LENGTH ? 0.0f : offsetZ / length;
    // only a grounded actor is lifted
    const float vertical = selfOnGround ? preset.mVerticalKickMultiplier : 0.0f;
    return Vector3f(preset.mPushScaleSelf * (-directionX * speed), preset.mPushScaleSelf * (vertical * speed),
                    preset.mPushScaleSelf * (speed * -directionZ));
}
