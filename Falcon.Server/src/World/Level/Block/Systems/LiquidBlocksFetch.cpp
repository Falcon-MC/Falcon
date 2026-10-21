#include "World/Level/Block/Systems/LiquidBlocksFetch.h"

#include "World/Level/Level.h"

#include <cmath>

namespace {
    const float PLAYER_BASE_OFFSET = 1.62f;
    const float PLAYER_HALF_WIDTH = 0.3f;
    const float PLAYER_HEIGHT = 1.8f;
    const float AABB_EPSILON = 0.001f;
}

LiquidContact LiquidBlocksFetch::at(Level &level, const Vector3f &feet, bool horizontalPose) {
    LiquidContact contact;
    Vector3f waterFlow;
    Vector3f lavaFlow;
    // BDS horizontal pose uses width as height (0x141002DC0), with eyes at 0.4 (0x140F1A3A0).
    const float height = horizontalPose ? PLAYER_HALF_WIDTH * 2.0f : PLAYER_HEIGHT;
    const float eyeHeight = horizontalPose ? 0.4f : PLAYER_BASE_OFFSET;
    const int32_t minX = (int32_t) std::floor(feet.x - PLAYER_HALF_WIDTH);
    const int32_t maxX = (int32_t) std::floor(feet.x + PLAYER_HALF_WIDTH);
    const int32_t minY = (int32_t) std::floor(feet.y);
    const int32_t maxY = (int32_t) std::floor(feet.y + height - AABB_EPSILON);
    const int32_t minZ = (int32_t) std::floor(feet.z - PLAYER_HALF_WIDTH);
    const int32_t maxZ = (int32_t) std::floor(feet.z + PLAYER_HALF_WIDTH);

    for (int32_t x = minX; x <= maxX; ++x) {
        for (int32_t y = minY; y <= maxY; ++y) {
            for (int32_t z = minZ; z <= maxZ; ++z) {
                const LiquidInfo info = level.getLiquidInfo(x, y, z);
                if (!info.water && !info.lava && !info.bubble)
                    continue;

                contact.water = contact.water || info.water || info.bubble;
                contact.lava = contact.lava || info.lava;
                contact.bubble = contact.bubble || info.bubble;
                contact.dragDown = contact.dragDown || (info.bubble && info.dragDown);
                if (info.water || info.lava) {
                    const Vector3f flow = level.getLiquidFlowVector(Vector3i(x, y, z));
                    Vector3f &total = info.lava ? lavaFlow : waterFlow;
                    total = Vector3f(total.x + flow.x, total.y + flow.y, total.z + flow.z);
                }
            }
        }
    }

    // BDS LiquidPhysicsSystem (0x140FC98E0) selects lava first and normalizes the summed current in 3D.
    contact.flow = contact.lava ? lavaFlow : waterFlow;
    const float flowLength = std::sqrt(contact.flow.x * contact.flow.x + contact.flow.y * contact.flow.y
                                        + contact.flow.z * contact.flow.z);
    if (flowLength >= 0.0001f)
        contact.flow = Vector3f(contact.flow.x / flowLength, contact.flow.y / flowLength, contact.flow.z / flowLength);
    else
        contact.flow = Vector3f();

    const float eyeY = feet.y + eyeHeight;
    const int32_t eyeX = (int32_t) std::floor(feet.x);
    const int32_t eyeBlockY = (int32_t) std::floor(eyeY);
    const int32_t eyeZ = (int32_t) std::floor(feet.z);
    const LiquidInfo eye = level.getLiquidInfo(eyeX, eyeBlockY, eyeZ);
    const float liquidTop = eye.bubble
                            ? (float) eyeBlockY + 1.0f
                            : (float) eyeBlockY + 1.0f - (eye.height - 0.1111111f);
    contact.eyeSubmerged = (eye.water || eye.bubble) && eyeY < liquidTop;
    contact.eyeInWater = eye.water && !eye.bubble && eyeY < liquidTop;

    const int32_t feetBlockY = (int32_t) std::floor(feet.y);
    const LiquidInfo feetInfo = level.getLiquidInfo(eyeX, feetBlockY, eyeZ);
    const float feetLiquidTop = (float) feetBlockY + 1.0f - (feetInfo.height - 0.1111111f);
    contact.feetInWater = feetInfo.water && !feetInfo.bubble && feet.y < feetLiquidTop;
    // SwimControlSystem samples the block at the Mob swim offset (0x1438827F0).
    const LiquidInfo probe = level.getLiquidInfo(eyeX, (int32_t) std::floor(feet.y + height * 0.85f), eyeZ);
    contact.swimProbeInWater = probe.water || probe.bubble;
    return contact;
}
