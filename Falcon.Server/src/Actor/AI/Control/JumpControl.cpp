#include "Actor/AI/Control/JumpControl.h"

#include "Actor/Mob/MobActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Network/Handler/ServerNetworkHandler.h"

void JumpControl::jump(float height) {
    mJumping = true;
    mHeight = height;
}

void JumpControl::tick(ServerNetworkHandler &owner, MobActor &mob) {
    mCoolDown++;

    if (!mJumping)
        return;

    mJumping = false;
    const bool inWater = LiquidBlocksFetch::at(owner.getLevelFor(mob), mob.getPosition()).water;

    Vector3f motion = mob.getMotion();
    motion.y += jumpingMotion(mHeight, inWater);
    mob.setMotion(motion);
    mCoolDown = 0;
}

float JumpControl::jumpingMotion(float height, bool inWater) {
    if (inWater)
        return 0.1f;
    if (height > 0.0f && height < 0.2f)
        return 0.15f;
    if (height < 0.51f)
        return 0.35f;
    if (height < 1.01f)
        return 0.5f;
    return 0.6f;
}
