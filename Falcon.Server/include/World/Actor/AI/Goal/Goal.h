#pragma once

#include <cstdint>

class MobActor;
class ServerNetworkHandler;

enum class GoalControlFlag : uint8_t {
    Move = 1 << 0,
    Look = 1 << 1,
    Jump = 1 << 2,
    Target = 1 << 3
};

class Goal {
public:
    virtual ~Goal() = default;

    virtual bool canUse(ServerNetworkHandler &owner, MobActor &mob) = 0;

    virtual bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
        return canUse(owner, mob);
    }

    virtual void start(ServerNetworkHandler &owner, MobActor &mob) {
        (void) owner;
        (void) mob;
    }

    virtual void stop(ServerNetworkHandler &owner, MobActor &mob) {
        (void) owner;
        (void) mob;
    }

    virtual void tick(ServerNetworkHandler &owner, MobActor &mob) {
        (void) owner;
        (void) mob;
    }

    uint8_t getRequiredControlFlags() const {
        return mRequiredControlFlags;
    }

protected:
    void setRequiredControlFlags(uint8_t flags) {
        mRequiredControlFlags = flags;
    }

private:
    uint8_t mRequiredControlFlags = 0;
};
