#include "Block/Blocks/DaylightDetectorBlock.h"

#include "Core/Math/MathConstants.h"
#include "Level/Level.h"
#include "Level/LightSystem.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const char *DAYLIGHT_DETECTOR = "minecraft:daylight_detector";
    const char *DAYLIGHT_DETECTOR_INVERTED = "minecraft:daylight_detector_inverted";
    const int SEARCH_RADIUS = 11;
}

DaylightDetectorBlock::DaylightDetectorBlock(const Block &base) : Block(base) {
}

bool DaylightDetectorBlock::matches(const std::string &identifier) {
    return identifier == DAYLIGHT_DETECTOR || identifier == DAYLIGHT_DETECTOR_INVERTED;
}

int DaylightDetectorBlock::effectiveSkyLightAround(Level &level, const Vector3i &position) {
    const int32_t skyReduction = level.getSkyLightSubtracted();

    int best = level.getSkyLightAt(position.x, position.y + 1, position.z) - skyReduction;
    if (best >= LightSystem::MAX_LIGHT)
        return LightSystem::MAX_LIGHT;

    for (int dx = -SEARCH_RADIUS; dx <= SEARCH_RADIUS; ++dx) {
        for (int dz = -SEARCH_RADIUS; dz <= SEARCH_RADIUS; ++dz) {
            const int distance = std::abs(dx) + std::abs(dz);
            if (distance == 0 || distance > SEARCH_RADIUS)
                continue;

            const int skyLight = level.getSkyLightAt(position.x + dx, position.y + 1, position.z + dz);
            const int signal = skyLight - skyReduction - distance;

            if (signal > best) {
                best = signal;
                if (best >= LightSystem::MAX_LIGHT)
                    return LightSystem::MAX_LIGHT;
            }
        }
    }

    return std::max(0, best);
}

int DaylightDetectorBlock::computeSignal(Level &level, const Vector3i &position, bool inverted) {
    int signal = effectiveSkyLightAround(level, position) - level.getSkyLightSubtracted();

    float angle = LightSystem::calculateCelestialAngle(level.getTime()) * MathConstants::TWO_PI_F;

    if (signal > 0) {
        const float target = angle < MathConstants::PI_F ? 0.0f : MathConstants::TWO_PI_F;
        angle = angle + (target - angle) * 0.2f;
        signal = (int) std::lround((float) signal * std::cos(angle));
    }

    signal = std::max(0, std::min(LightSystem::MAX_LIGHT, signal));

    if (inverted)
        signal = LightSystem::MAX_LIGHT - signal;

    return signal;
}

bool DaylightDetectorBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player,
                                       const Vector3i &position, const BlockState &state) const {
    (void) player;

    BlockState toggled = state;
    toggled.mName = state.mName == DAYLIGHT_DETECTOR ? DAYLIGHT_DETECTOR_INVERTED : DAYLIGHT_DETECTOR;

    owner.getLevel().setBlockState(position.x, position.y, position.z, toggled);
    return true;
}
