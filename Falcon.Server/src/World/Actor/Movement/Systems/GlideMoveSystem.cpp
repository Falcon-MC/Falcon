#include "World/Actor/Movement/Systems/GlideMoveSystem.h"
#include <algorithm>
#include <cmath>
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

using namespace MovementPhysics;

namespace MovementPhysics {
    // BDS GlideMoveSystem (0x140C66F70), before collision resolution.
    void glide(Vector3f &velocity, const PlayerAuthInputPacket &packet, bool slowFalling) {
        const float pitch = packet.mRotation.x * MOJANG_PI / 180.0f;
        const float yaw = packet.mRotation.y * MOJANG_PI / 180.0f;
        const float cosine = mojangCos(pitch);
        const Vector3f look(-mojangSin(yaw) * cosine, -mojangSin(pitch), mojangCos(yaw) * cosine);
        const float horizontalLook = std::sqrt(look.x * look.x + look.z * look.z);
        const float horizontalSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
        const float lift = std::min(std::sqrt(look.x * look.x + look.y * look.y + look.z * look.z) * 2.5f, 1.0f)
                           * cosine * cosine;
        velocity.y -= (slowFalling ? -0.0075f : -0.06f) * lift - (slowFalling ? -0.01f : -0.08f);
        if (velocity.y < 0.0f && horizontalLook > 0.0f) {
            const float fallLift = velocity.y * -0.1f * lift;
            velocity.x += fallLift * look.x * (1.0f / horizontalLook);
            velocity.y += fallLift;
            velocity.z += fallLift * look.z * (1.0f / horizontalLook);
        }
        if (pitch < 0.0f && horizontalLook > 0.0f) {
            const float climb = horizontalSpeed * mojangSin(pitch) * -0.04f;
            velocity.x -= climb * look.x * (1.0f / horizontalLook);
            velocity.y += climb * 3.2f;
            velocity.z -= climb * look.z * (1.0f / horizontalLook);
        }
        if (horizontalLook > 0.0f) {
            velocity.x += (look.x * (1.0f / horizontalLook) * horizontalSpeed - velocity.x) * 0.1f;
            velocity.z += (look.z * (1.0f / horizontalLook) * horizontalSpeed - velocity.z) * 0.1f;
        }
        velocity.x *= 0.99f;
        velocity.y *= 0.98f;
        velocity.z *= 0.99f;
    }
}

void GlideMoveSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    if (abilities.gliding && !contact.waterTravel && !contact.lavaTravel)
        glide(state.velocity, input.packet, context.player.hasEffect(MobEffectId::SlowFalling));
}
