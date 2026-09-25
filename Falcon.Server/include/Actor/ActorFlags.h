#pragma once

#include <cstdint>

enum class ActorFlag : int {
    OnFire = 0,
    Sneaking = 1,
    Riding = 2,
    Sprinting = 3,
    UsingItem = 4,
    Invisible = 5,
    InLove = 7,
    Powered = 9,
    Ignited = 10,
    Baby = 11,
    CanClimb = 19,
    CanWalk = 22,
    Sitting = 24,
    Angry = 25,
    Tamed = 28,
    Sheared = 31,
    Gliding = 32,
    Moving = 34,
    Breathing = 35,
    HasCollision = 48,
    HasGravity = 49,
    SpinAttack = 56,
    Swimming = 57,
    Blocking = 72,
    TransitionBlocking = 73,
    BlockedUsingShield = 74,
    BlockedUsingDamagedShield = 75,
    Sleeping = 76,
    Crawling = 114,
};

class ActorFlags {
public:
    static const int32_t FLAGS_DATA_ID = 0;
    static const int32_t FLAGS_2_DATA_ID = 91;
    static const int32_t VARIANT_DATA_ID = 2;
    static const int32_t COLOR_DATA_ID = 3;
    static const int32_t OWNER_DATA_ID = 5;
    static const int32_t SCALE_DATA_ID = 38;
    static const int32_t MARK_VARIANT_DATA_ID = 43;
    static const int32_t PLAYER_FLAGS_DATA_ID = 26;
    static const int32_t BED_POSITION_DATA_ID = 28;
    static const int32_t FUSE_LENGTH_DATA_ID = 55;
    static const int32_t AIR_SUPPLY_DATA_ID = 7;
    static const int32_t AIR_SUPPLY_MAX_DATA_ID = 42;
    static const int32_t VISIBLE_MOB_EFFECTS_DATA_ID = 131;
    static const int8_t PLAYER_FLAG_SLEEP = 0x2;

    void set(ActorFlag flag, bool value);

    bool get(ActorFlag flag) const;

    int64_t getLowBits() const { return mLow; }

    int64_t getHighBits() const { return mHigh; }

    void applyPlayerDefaults();

private:
    int64_t mLow = 0;
    int64_t mHigh = 0;
};
