#pragma once

#include <cstdint>

class MobActor;
class ServerNetworkHandler;

class JumpControl {
public:
    static constexpr int32_t JUMP_COOL_DOWN = 10;

    void jump(float height);

    bool isCoolingDown() const {
        return mCoolDown <= JUMP_COOL_DOWN;
    }

    void tick(ServerNetworkHandler &owner, MobActor &mob);

    static float jumpingMotion(float height, bool inWater);

private:
    int32_t mCoolDown = JUMP_COOL_DOWN + 1;
    float mHeight = 0.0f;
    bool mJumping = false;
};
