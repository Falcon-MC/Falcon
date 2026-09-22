#pragma once

class MobActor;
class ServerNetworkHandler;

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
};
