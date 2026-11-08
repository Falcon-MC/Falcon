#include "World/Actor/SunlightBurn.h"

#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const int32_t BURN_TICKS = 8 * 20;
    const float MIN_BRIGHTNESS = 0.5f;
    const float MAX_LIGHT = 15.0f;
    const float EYE_HEIGHT_RATIO = 0.85f;
}

// no random roll: the game sets fire on the first tick the actor stands in daylight
void SunlightBurn::tick(ServerNetworkHandler &owner, ServerActor &actor) {
    if (!actor.burnsInDaylight() || actor.isOnFire() || actor.isFireImmune()
        || actor.hasEffect(MobEffectId::FireResistance))
        return;

    Level &level = owner.getLevelFor(actor);
    if (!level.hasSkyLight() || level.isRaining())
        return;

    const Vector3f position = actor.getPosition();
    if (LiquidBlocksFetch::at(level, position).water)
        return;

    // the top of the box has to reach the heightmap: sub_140BAEA60
    const int32_t top = (int32_t) std::round(position.y + actor.getSize().mHeight);
    const int32_t blockX = (int32_t) std::floor(position.x);
    const int32_t blockZ = (int32_t) std::floor(position.z);
    if (level.getHeightAt(blockX, blockZ) > top)
        return;

    const int32_t eyeY = (int32_t) std::floor(position.y + actor.getSize().mHeight * EYE_HEIGHT_RATIO);
    const float brightness = (float) std::max(0, level.getSkyLightAt(blockX, eyeY, blockZ)
                                                 - level.getSkyLightSubtracted()) / MAX_LIGHT;
    if (brightness <= MIN_BRIGHTNESS)
        return;

    if (actor.shieldFromSunlight())
        return;

    actor.setFireTicks(BURN_TICKS);
    actor.setOnFire(true);
    owner.syncActorFlags(actor);
}
