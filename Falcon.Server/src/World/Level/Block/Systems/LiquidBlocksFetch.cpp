#include "World/Level/Block/Systems/LiquidBlocksFetch.h"

#include "World/Level/Level.h"

#include <cmath>

namespace {
    const float PLAYER_HALF_WIDTH = 0.3f;
}

LiquidContact LiquidBlocksFetch::at(Level &level, const Vector3f &feet, bool horizontalPose, float bodyHeight) {
    LiquidContact contact;
    Vector3f waterFlow;
    Vector3f lavaFlow;
    // BDS horizontal pose uses width as height (0x141002DC0), with eyes at 0.4 (0x140F1A3A0).
    const float height = horizontalPose ? PLAYER_HALF_WIDTH * 2.0f : bodyHeight;
    // BDS 0x140F1A3A0 uses a separate eye offset for modern crouching, not the body height.
    const float eyeHeight = horizontalPose ? 0.4f : bodyHeight < 1.8f ? 1.62001f - 0.35f : 1.62001f;
    // BDS 0x140FC98E0 shrinks players' liquid sensing boxes; only ItemActorFlagComponent skips this.
    // Water uses (0.001, 0.401, 0.001), lava (0.1, 0.4, 0.1), collapsing inverted axes to the center.
    const auto scan = [&](bool lava) {
        const float horizontalInset = lava ? 0.1f : 0.001f;
        const float verticalInset = lava ? 0.4f : 0.401f;
        const float bottom = height < verticalInset * 2.0f ? feet.y + height * 0.5f : feet.y + verticalInset;
        const float top = height < verticalInset * 2.0f ? feet.y + height * 0.5f : feet.y + height - verticalInset;
        const int32_t minX = (int32_t) std::floor(feet.x - PLAYER_HALF_WIDTH + horizontalInset);
        const int32_t maxX = (int32_t) std::floor(feet.x + PLAYER_HALF_WIDTH - horizontalInset);
        const int32_t minY = (int32_t) std::floor(bottom);
        const int32_t maxY = (int32_t) std::floor(top);
        const int32_t minZ = (int32_t) std::floor(feet.z - PLAYER_HALF_WIDTH + horizontalInset);
        const int32_t maxZ = (int32_t) std::floor(feet.z + PLAYER_HALF_WIDTH - horizontalInset);
        for (int32_t x = minX; x <= maxX; ++x) {
            for (int32_t y = minY; y <= maxY; ++y) {
                for (int32_t z = minZ; z <= maxZ; ++z) {
                    const LiquidInfo info = level.getLiquidInfo(x, y, z);
                    if (lava ? !info.lava : !info.water && !info.bubble)
                        continue;
                    if (lava) {
                        contact.lava = true;
                    } else {
                        contact.water = true;
                        contact.bubble = contact.bubble || info.bubble;
                        contact.dragDown = contact.dragDown || (info.bubble && info.dragDown);
                    }
                    if (info.water || info.lava) {
                        const Vector3f flow = level.getLiquidFlowVector(Vector3i(x, y, z));
                        Vector3f &total = lava ? lavaFlow : waterFlow;
                        total = Vector3f(total.x + flow.x, total.y + flow.y, total.z + flow.z);
                    }
                }
            }
        }
    };
    scan(false);
    scan(true);

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
    // BDS PlayerBoundingBoxStateUpdateSystem (0x140F97340) stores the head block's
    // liquid material in PlayerInputRequest +33, independently of the fluid surface height.
    contact.swimProbeInWater = eye.water || eye.bubble || eye.lava;
    return contact;
}
