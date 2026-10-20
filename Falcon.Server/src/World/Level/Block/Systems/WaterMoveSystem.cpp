#include "World/Level/Block/Systems/WaterMoveSystem.h"

#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>

void WaterMoveSystem::tick(ServerPlayer &player, const LiquidContact &contact) {
    if (!contact.water && !contact.bubble)
        return;

    player.resetFallDistance();
    Vector3f motion = player.getMotion();
    if (contact.water || contact.lava) {
        motion.x += contact.flow.x * 0.014f;
        motion.z += contact.flow.z * 0.014f;
    }

    if (contact.bubble) {
        const float acceleration = contact.dragDown ? -0.03f : 0.06f;
        const float maximum = contact.dragDown ? -0.3f : 0.7f;
        motion.y = contact.dragDown ? std::max(maximum, motion.y + acceleration)
                                    : std::min(maximum, motion.y + acceleration);
    }
    motion.x = std::clamp(motion.x, -0.2f, 0.2f);
    motion.z = std::clamp(motion.z, -0.2f, 0.2f);
    player.setMotion(motion);

    if (player.isOnFire())
        player.extinguish();
}
