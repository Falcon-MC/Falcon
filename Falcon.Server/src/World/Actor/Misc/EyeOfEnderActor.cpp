#include "World/Actor/Misc/EyeOfEnderActor.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Feature/FeatureMath.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

FALCON_REGISTER_ACTOR(EyeOfEnderActor, EyeOfEnderActor::IDENTIFIER);

namespace {
    // farther than this the eye aims at a point this far along the way, 8 blocks over its start
    const float MAX_REACH = 12.0f;
    const float RISE = 8.0f;
    const float SPEED_GAIN = 0.0025f;
    const float CLOSE = 1.0f;
    const float CLOSE_BRAKE = 0.8f;
    const float CLIMB_GAIN = 0.015f;
    const float SINE_SCALE = 10430.378f;
    const float QUARTER_TURN_SCALED = 16384.0f;
    // the level event of the shattering eye, then the sound event the game sends with it
    const int32_t SHATTER_EVENT = 2003;
    const int32_t SHATTER_EVENT_DATA = 10;
    const int32_t SHATTER_SOUND_EVENT = 1041;
    const int32_t SURVIVE_ROLL = 5;
}

void EyeOfEnderActor::signalTo(const Vector3i &target, std::mt19937 &random) {
    const Vector3f &position = getPosition();
    const float targetX = (float) target.x;
    const float targetZ = (float) target.z;
    const float dx = targetX - position.x;
    const float dz = targetZ - position.z;
    const float distance = std::sqrt(dz * dz + dx * dx);
    if (distance <= MAX_REACH)
        mTarget = Vector3f(targetX, (float) target.y, targetZ);
    else
        mTarget = Vector3f(position.x + dx / distance * MAX_REACH, position.y + RISE,
                           dz / distance * MAX_REACH + position.z);
    mLife = 0;
    mSurviveAfterDeath = GameRandom::nextInt(random, SURVIVE_ROLL) > 0;
}

void EyeOfEnderActor::step(Vector3f &position, Vector3f &motion, const Vector3f &target) {
    position = Vector3f(position.x + motion.x, position.y + motion.y, position.z + motion.z);

    const float dx = target.x - position.x;
    const float dz = target.z - position.z;
    const float distance = std::sqrt(dz * dz + dx * dx);
    const float speed = std::sqrt(motion.z * motion.z + motion.x * motion.x);
    const float angle = std::atan2(dz, dx);
    float nextSpeed = (distance - speed) * SPEED_GAIN + speed;
    float climb = motion.y;
    if (distance < CLOSE) {
        nextSpeed *= CLOSE_BRAKE;
        climb *= CLOSE_BRAKE;
    }

    const float scaled = angle * SINE_SCALE;
    const float towards = target.y > position.y ? 1.0f : -1.0f;
    motion = Vector3f(FeatureMath::sineAt(scaled + QUARTER_TURN_SCALED) * nextSpeed,
                      (towards - climb) * CLIMB_GAIN + climb, nextSpeed * FeatureMath::sineAt(scaled));
}

void EyeOfEnderActor::tick(ServerNetworkHandler &owner) {
    if (mExpired)
        return;

    Vector3f position = getPosition();
    Vector3f motion = getMotion();
    step(position, motion, mTarget);
    setPosition(position);
    setMotion(motion);
    owner.broadcastActorMove(*this);
    owner.sendActorMotion(*this);

    // the flight runs before the life count: the last step is still taken
    if (mLife++ < LIFETIME)
        return;

    mExpired = true;
    Level &level = owner.getLevelFor(*this);
    if (mSurviveAfterDeath) {
        owner.spawnItemActor(level, "minecraft:ender_eye", 1, position);
        return;
    }
    owner.broadcastLevelEvent(level, SHATTER_EVENT, position, SHATTER_EVENT_DATA);
    owner.broadcastLevelEvent(level, SHATTER_SOUND_EVENT, position, 0);
}
