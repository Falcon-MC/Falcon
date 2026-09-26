#include "Actor/Definition/LookedAtSensor.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Core/Math/MathConstants.h"
#include "Level/Explosion.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace {
    const char *const LOOKED_AT_COMPONENT = "minecraft:looked_at";
    const char *const SET_TARGET_NEVER = "never";
    const char *const SET_TARGET_ONCE_AND_STOP = "once_and_stop_scanning";
    const char *const HEAD_LOCATION = "head";
    const char *const BODY_LOCATION = "body";
    const char *const FEET_LOCATION = "feet";
    const float DEFAULT_SEARCH_RADIUS = 10.0f;
    const float DEFAULT_FIELD_OF_VIEW = 26.0f;
    const float PLAYER_EYE_HEIGHT = 1.62f;
    const float HEAD_HEIGHT_RATIO = 0.85f;
    const float BODY_HEIGHT_RATIO = 0.5f;
    const double SIGHT_STEP = 0.25;
    const int32_t TICKS_PER_SECOND = 20;

    float numberOr(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    bool flagOr(const json::Value &component, const char *key, bool fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }

    std::string stringOr(const json::Value &component, const char *key, const char *fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? std::string(fallback) : value->string();
    }

    int32_t secondsToTicks(float seconds) {
        return (int32_t) std::lround(seconds * (float) TICKS_PER_SECOND);
    }

    float locationHeight(const MobActor &mob, const std::string &location) {
        const float height = mob.getSize().mHeight;
        if (location == FEET_LOCATION)
            return 0.0f;
        if (location == BODY_LOCATION)
            return height * BODY_HEIGHT_RATIO;
        return height * HEAD_HEIGHT_RATIO;
    }

    std::string locationName(const json::Value &entry) {
        if (entry.isString())
            return entry.mString;

        const json::Value *location = entry.get("location");
        return location == nullptr ? std::string(HEAD_LOCATION) : location->string();
    }
}

void LookedAtSensor::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(LOOKED_AT_COMPONENT);
    if (component != mComponent) {
        mComponent = component;
        mNextScanTick = 0;
        mStopped = false;
        mLookTicks.clear();
    }

    if (component == nullptr)
        return;

    const int64_t now = owner.getCurrentTick();
    if (now < mNextScanTick)
        return;

    const int32_t interval = std::max(1, secondsToTicks(numberOr(*component, "looked_at_cooldown", 0.0f)));
    mNextScanTick = now + interval;

    if (mStopped) {
        if (mob.getTarget(owner) != nullptr)
            return;
        mStopped = false;
    }

    ServerPlayer *looker = _findLooker(owner, mob, *component, interval);
    if (looker == nullptr) {
        EntityEvents::fireTrigger(owner, mob, component->get("not_looked_at_event"));
        return;
    }

    const std::string setTarget = stringOr(*component, "set_target", SET_TARGET_ONCE_AND_STOP);
    if (setTarget != SET_TARGET_NEVER) {
        if (mob.getTarget(owner) == nullptr)
            mob.setTarget(owner, looker->getRuntimeId());
        mStopped = setTarget == SET_TARGET_ONCE_AND_STOP;
    }

    EntityEvents::fireTrigger(owner, mob, component->get("looked_at_event"), looker);
}

ServerPlayer *LookedAtSensor::_findLooker(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component,
                                          int32_t interval) {
    const float radius = numberOr(component, "search_radius", DEFAULT_SEARCH_RADIUS);
    const float radiusSquared = radius * radius;
    const int32_t minimumTicks = secondsToTicks(numberOr(component, "min_looked_at_duration", 0.0f));
    const json::Value *filters = component.get("filters");
    Level &level = owner.getLevelFor(mob);

    std::unordered_map<uint64_t, int32_t> lookTicks;
    ServerPlayer *nearest = nullptr;
    float nearestDistance = radiusSquared;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!mob.canTarget(player))
            continue;

        const float distance = mob.distanceSquaredTo(player);
        if (distance > radiusSquared)
            continue;

        if (filters != nullptr && !EntityFilter::test(*filters, owner, mob, &player))
            continue;

        if (!_isLookingAt(level, mob, player, component))
            continue;

        if (minimumTicks > 0) {
            const auto previous = mLookTicks.find(player.getRuntimeId());
            const int32_t ticks = (previous == mLookTicks.end() ? 0 : previous->second) + interval;
            lookTicks[player.getRuntimeId()] = ticks;
            if (ticks < minimumTicks)
                continue;
        }

        if (nearest == nullptr || distance < nearestDistance) {
            nearest = &player;
            nearestDistance = distance;
        }
    }

    mLookTicks.swap(lookTicks);
    return nearest;
}

bool LookedAtSensor::_isLookingAt(Level &level, const MobActor &mob, const ServerPlayer &player,
                                  const json::Value &component) {
    const Vector3f &rotation = player.getRotation();
    const float pitch = rotation.x * MathConstants::DEGREES_TO_RADIANS_F;
    const float yaw = rotation.y * MathConstants::DEGREES_TO_RADIANS_F;
    const Vector3f direction(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    const Vector3f feet = player.getPosition();
    const Vector3f eye(feet.x, feet.y + PLAYER_EYE_HEIGHT, feet.z);

    const float halfAngle = numberOr(component, "field_of_view", DEFAULT_FIELD_OF_VIEW) * 0.5f
                            * MathConstants::DEGREES_TO_RADIANS_F;
    const float tolerance = 1.0f - std::cos(halfAngle);
    const bool scaleByDistance = flagOr(component, "scale_fov_by_distance", true);
    const Vector3f position = mob.getPosition();

    const json::Value *locations = component.get("look_at_locations");
    if (locations == nullptr || !locations->isArray() || locations->mArray.empty()) {
        const Vector3f head(position.x, position.y + locationHeight(mob, HEAD_LOCATION), position.z);
        return _canSee(level, eye, direction, head, tolerance, scaleByDistance);
    }

    for (const std::unique_ptr<json::Value> &entry: locations->mArray) {
        const float offset = entry->isObject() ? numberOr(*entry, "vertical_offset", 0.0f) : 0.0f;
        const float height = locationHeight(mob, locationName(*entry)) + offset;
        if (_canSee(level, eye, direction, Vector3f(position.x, position.y + height, position.z), tolerance,
                    scaleByDistance))
            return true;
    }
    return false;
}

bool LookedAtSensor::_canSee(Level &level, const Vector3f &eye, const Vector3f &direction, const Vector3f &point,
                             float tolerance, bool scaleByDistance) {
    const float dx = point.x - eye.x;
    const float dy = point.y - eye.y;
    const float dz = point.z - eye.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance <= 0.0f)
        return true;

    const float dot = (dx * direction.x + dy * direction.y + dz * direction.z) / distance;
    const float threshold = 1.0f - (scaleByDistance ? tolerance / distance : tolerance);
    if (dot <= threshold)
        return false;

    return !Explosion::isRayCollidingWithBlocks(level, eye.x, eye.y, eye.z, point.x, point.y, point.z, SIGHT_STEP);
}
