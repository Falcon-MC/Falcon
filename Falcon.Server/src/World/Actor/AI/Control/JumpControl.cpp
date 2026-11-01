#include "World/Actor/AI/Control/JumpControl.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const float JUMP_PROBE_DEPTH = 0.5f;
    const float FULL_JUMP_HEIGHT = 0.51f;
}

void JumpControl::jump(float height) {
    mJumping = true;
    mHeight = height;
}

void JumpControl::tick(ServerNetworkHandler &owner, MobActor &mob) {
    mCoolDown++;

    if (!mJumping)
        return;

    mJumping = false;
    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    const bool inWater = LiquidBlocksFetch::at(level, position).water;

    float jumpFactor = 1.0f;
    const BlockState *below = level.peekBlockPtr((int32_t) std::floor(position.x),
                                                 (int32_t) std::floor(position.y - JUMP_PROBE_DEPTH),
                                                 (int32_t) std::floor(position.z));
    const Block *block = below == nullptr ? nullptr : VanillaBlocks::fromIdentifier(below->mName);
    if (block != nullptr)
        jumpFactor = block->getJumpFactor();

    // A full-block obstacle takes the mob's jump power, which clears about 1.25 blocks: stronger impulses
    // would carry chickens and armadillos over fences.
    const bool fullJump = !inWater && mHeight >= FULL_JUMP_HEIGHT;
    Vector3f motion = mob.getMotion();
    motion.y += (fullJump ? mob.getJumpPower() : jumpingMotion(mHeight, inWater)) * jumpFactor;
    mob.setMotion(motion);
    mCoolDown = 0;
}

float JumpControl::jumpingMotion(float height, bool inWater) {
    if (inWater)
        return 0.1f;
    if (height > 0.0f && height < 0.2f)
        return 0.15f;
    return 0.35f;
}
