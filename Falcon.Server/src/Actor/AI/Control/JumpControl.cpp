#include "Actor/AI/Control/JumpControl.h"

#include "Actor/Mob/MobActor.h"
#include "Block/Block.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const float JUMP_PROBE_DEPTH = 0.5f;
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

    Vector3f motion = mob.getMotion();
    motion.y += jumpingMotion(mHeight, inWater) * jumpFactor;
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
