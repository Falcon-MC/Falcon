#include "World/Actor/Movement/MovementPhysics.h"
#include "World/Level/Level.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/VanillaBlocks.h"

#include <algorithm>
#include <cmath>

namespace MovementPhysics {
    /**
     * Single-precision sine with the client's own range reduction and polynomial. The result
     * feeds the sine table below, so it has to match the client bit for bit or simulated
     * positions drift away from the client's prediction and trigger needless corrections.
     */
    float mojangFloatSin(float value) {
        if (value == 0.0f || std::isnan(value))
            return value;
        if (std::isinf(value))
            return NAN;

        bool negative = false;
        if (value < 0.0f) {
            value = -value;
            negative = true;
        }

        long long octant = (long long) (value * (4.0f / MOJANG_PI));
        float octantValue = (float) octant;
        if ((octant & 1LL) == 1LL) {
            octant++;
            octantValue++;
        }
        octant &= 7LL;
        float reduced = ((value - octantValue * 0.7853981f) - octantValue * 3.7748947e-8f)
                        - octantValue * 2.6951514e-15f;
        if (octant > 3LL) {
            negative = !negative;
            octant -= 4LL;
        }

        const float squared = reduced * reduced;
        float result;
        if (octant == 1LL || octant == 2LL) {
            result = 1.0f - 0.5f * squared + squared * squared
                     * (((((COS_COEFFICIENTS[0] * squared + COS_COEFFICIENTS[1]) * squared + COS_COEFFICIENTS[2])
                          * squared + COS_COEFFICIENTS[3]) * squared + COS_COEFFICIENTS[4]) * squared
                        + COS_COEFFICIENTS[5]);
        } else {
            result = reduced + reduced * squared
                     * (((((SIN_COEFFICIENTS[0] * squared + SIN_COEFFICIENTS[1]) * squared + SIN_COEFFICIENTS[2])
                          * squared + SIN_COEFFICIENTS[3]) * squared + SIN_COEFFICIENTS[4]) * squared
                        + SIN_COEFFICIENTS[5]);
        }
        return negative ? -result : result;
    }

    const float *mojangSineTable() {
        static float table[65536];
        static bool ready = false;
        if (!ready) {
            for (int index = 0; index < 65536; index++) {
                const float angle = ((float) index * MOJANG_PI * 2.0f) / 65536.0f;
                table[index] = mojangFloatSin(angle);
            }
            ready = true;
        }
        return table;
    }

    /**
     * The client looks movement trigonometry up in a 65536-entry table rather than calling
     * sin/cos; 10430.378 is 65536 / 2pi, and cosine is the same table shifted a quarter turn.
     */
    float mojangSin(float radians) {
        return mojangSineTable()[(int) (radians * 10430.378f) & 65535];
    }

    float mojangCos(float radians) {
        return mojangSineTable()[(int) (radians * 10430.378f + 16384.0f) & 65535];
    }

    bool input(const PlayerAuthInputPacket &packet, PlayerAuthInputData flag) {
        return packet.hasInputFlag((int32_t) flag);
    }

    bool activeState(bool previous, const PlayerAuthInputPacket &packet,
                     PlayerAuthInputData start, PlayerAuthInputData stop) {
        return (previous || input(packet, start)) && !input(packet, stop);
    }

    bool chunkLoaded(Level &level, const Vector3f &position) {
        return level.getBlockActors().isChunkLoaded((int32_t) std::floor(position.x) >> 4,
                                                    (int32_t) std::floor(position.z) >> 4);
    }

    bool collides(Level &level, const AxisAlignedBB &box) {
        for (const AxisAlignedBB &obstacle: level.getCollisionBoxes(box)) {
            if (obstacle.intersectsWith(box))
                return true;
        }
        return false;
    }

    AxisAlignedBB boxAt(const Vector3f &feet, float height) {
        const float half = PLAYER_WIDTH * 0.5f;
        return AxisAlignedBB(feet.x - half, feet.y, feet.z - half, feet.x + half, feet.y + height, feet.z + half);
    }

    const Block *blockAt(Level &level, float x, float y, float z) {
        const BlockState *state = level.peekBlockPtr((int32_t) std::floor(x), (int32_t) std::floor(y),
                                                     (int32_t) std::floor(z));
        return state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
    }

    void moveRelative(Vector3f &velocity, float sideways, float forward, float speed, float yawDegrees) {
        float force = sideways * sideways + forward * forward;
        if (force < 1.0e-4f)
            return;
        force = speed / std::max(std::sqrt(force), 1.0f);
        sideways *= force;
        forward *= force;
        const float yaw = yawDegrees * MOJANG_PI / 180.0f;
        const float sine = mojangSin(yaw);
        const float cosine = mojangCos(yaw);
        velocity.x += sideways * cosine - forward * sine;
        velocity.z += forward * cosine + sideways * sine;
    }
}
