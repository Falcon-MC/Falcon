#include "Block/Blocks/HoneyBlock.h"

#include "Actor/Actor.h"
#include "Actor/ServerActor.h"
#include "Block/BlockClassRegistry.h"

#include <cmath>

FALCON_REGISTER_BLOCK(HoneyBlock, 185);

namespace {
    const float SLIDE_MAX_UPWARD_MOTION = 0.08f;
    const float SLIDE_FALL_SPEED = -0.05f;
    const float SLIDE_BRAKE_THRESHOLD = -0.13f;
    const float SIDE_HALF_WIDTH = 0.4375f;
    const float SIDE_EPSILON = 1.0e-3f;
}

bool HoneyBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:honey_block";
}

void HoneyBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                               const BlockState &state) const {
    (void) owner;
    (void) state;
    const Vector3f motion = actor.getMotion();
    if (actor.isOnGround() || motion.y > SLIDE_MAX_UPWARD_MOTION)
        return;

    const ServerActor *serverActor = dynamic_cast<const ServerActor *>(&actor);
    const float halfWidth = serverActor == nullptr ? 0.3f : serverActor->getSize().mWidth * 0.5f;
    const Vector3f actorPosition = actor.getPosition();
    const float offsetX = std::fabs((float) position.x + 0.5f - actorPosition.x);
    const float offsetZ = std::fabs((float) position.z + 0.5f - actorPosition.z);
    const float width = SIDE_HALF_WIDTH + halfWidth;
    if (offsetX + SIDE_EPSILON <= width && offsetZ + SIDE_EPSILON <= width)
        return;

    Vector3f slid = motion;
    slid.y = SLIDE_FALL_SPEED;
    if (motion.y < SLIDE_BRAKE_THRESHOLD) {
        const float brake = SLIDE_FALL_SPEED / motion.y;
        slid.x *= brake;
        slid.z *= brake;
    }
    actor.setMotion(slid);
    actor.resetFallDistance();
}
