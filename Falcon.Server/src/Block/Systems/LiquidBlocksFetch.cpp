#include "Block/Systems/LiquidBlocksFetch.h"

#include "Level/Level.h"

#include <cmath>

namespace {
    const float PLAYER_BASE_OFFSET = 1.62f;
    const float PLAYER_HALF_WIDTH = 0.3f;
    const float PLAYER_HEIGHT = 1.8f;
    const float AABB_EPSILON = 0.001f;
}

LiquidContact LiquidBlocksFetch::at(Level &level, const Vector3f &feet) {
    LiquidContact contact;
    const int32_t minX = (int32_t) std::floor(feet.x - PLAYER_HALF_WIDTH);
    const int32_t maxX = (int32_t) std::floor(feet.x + PLAYER_HALF_WIDTH);
    const int32_t minY = (int32_t) std::floor(feet.y);
    const int32_t maxY = (int32_t) std::floor(feet.y + PLAYER_HEIGHT - AABB_EPSILON);
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
                    contact.flow = Vector3f(contact.flow.x + flow.x, contact.flow.y + flow.y,
                                            contact.flow.z + flow.z);
                }
            }
        }
    }

    const float eyeY = feet.y + PLAYER_BASE_OFFSET;
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
    return contact;
}
