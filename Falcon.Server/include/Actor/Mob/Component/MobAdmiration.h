#pragma once

#include <cstdint>

class ItemStack;
class MobActor;
class ServerNetworkHandler;

class MobAdmiration {
public:
    bool isAdmiring() const {
        return mTicks > 0;
    }

    bool canAdmire(ServerNetworkHandler &owner, const MobActor &mob) const;

    void start(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item, bool barter);

    void tick(ServerNetworkHandler &owner, MobActor &mob);

    void abort(ServerNetworkHandler &owner, MobActor &mob);

private:
    void _finish(ServerNetworkHandler &owner, MobActor &mob);

    int32_t mTicks = 0;
    bool mBarter = false;
};
