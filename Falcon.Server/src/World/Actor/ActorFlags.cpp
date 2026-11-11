#include "World/Actor/ActorFlags.h"

void ActorFlags::set(ActorFlag flag, bool value) {
    const int bit = (int) flag;
    int64_t &target = mBits[bit / 64];
    const int64_t mask = (int64_t) 1 << (bit % 64);

    if (value)
        target |= mask;
    else
        target &= ~mask;
}

bool ActorFlags::get(ActorFlag flag) const {
    const int bit = (int) flag;
    const int64_t source = mBits[bit / 64];
    return (source & ((int64_t) 1 << (bit % 64))) != 0;
}

void ActorFlags::applyPlayerDefaults() {
    set(ActorFlag::CanWalk, true);
    set(ActorFlag::CanClimb, true);
    set(ActorFlag::Breathing, true);
    set(ActorFlag::HasCollision, true);
    set(ActorFlag::HasGravity, true);
    set(ActorFlag::PushTowardsClosestSpace, true);
}
