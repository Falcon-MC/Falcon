#include "World/Actor/SunlightBurn.h"

#include "World/Actor/ServerActor.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const int32_t BURN_TICKS = 8 * 20;
    // strictly above: sub_140BAEA60
    const float MIN_BRIGHTNESS = 0.5f;
    const float MAX_LIGHT = 15.0f;
    // the sky darkens past this at dusk and dawn: no burning
    const int32_t MAX_SKY_DARKEN = 3;
    const float WATER_BOX_INSET = 0.001f;

    // any water in the box shrunk by 0.001: sub_1432D9CB0
    bool touchesWater(Level &level, const ServerActor &actor) {
        const Vector3f position = actor.getPosition();
        const ActorSize size = actor.getSize();
        const float half = size.mWidth * 0.5f - WATER_BOX_INSET;
        const int32_t minX = (int32_t) std::floor(position.x - half);
        const int32_t maxX = (int32_t) std::floor(position.x + half);
        const int32_t minY = (int32_t) std::floor(position.y + WATER_BOX_INSET);
        const int32_t maxY = (int32_t) std::floor(position.y + size.mHeight - WATER_BOX_INSET);
        const int32_t minZ = (int32_t) std::floor(position.z - half);
        const int32_t maxZ = (int32_t) std::floor(position.z + half);
        for (int32_t x = minX; x <= maxX; ++x) {
            for (int32_t y = minY; y <= maxY; ++y) {
                for (int32_t z = minZ; z <= maxZ; ++z) {
                    if (level.getLiquidInfo(x, y, z).water)
                        return true;
                }
            }
        }
        return false;
    }
}

// no random roll: the game sets fire on the first tick the actor stands in daylight
void SunlightBurn::tick(ServerNetworkHandler &owner, ServerActor &actor) {
    if (!actor.burnsInDaylight() || actor.isOnFire() || actor.isFireImmune()
        || actor.hasEffect(MobEffectId::FireResistance))
        return;

    Level &level = owner.getLevelFor(actor);
    if (!level.hasSkyLight() || level.isRaining() || level.getSkyLightSubtracted() > MAX_SKY_DARKEN)
        return;

    if (actor.shieldFromSunlight())
        return;

    // the top of the box has to reach the heightmap: sub_140BAEA60
    const Vector3f position = actor.getPosition();
    const int32_t top = (int32_t) std::round(position.y + actor.getSize().mHeight);
    const int32_t blockX = (int32_t) std::floor(position.x);
    const int32_t blockY = (int32_t) std::floor(position.y);
    const int32_t blockZ = (int32_t) std::floor(position.z);
    if (level.getHeightAt(blockX, blockZ) > top)
        return;

    const float brightness = (float) std::max(0, level.getSkyLightAt(blockX, blockY, blockZ)
                                                         - level.getSkyLightSubtracted())
                             / MAX_LIGHT;
    if (brightness <= MIN_BRIGHTNESS || touchesWater(level, actor))
        return;

    actor.setFireTicks(BURN_TICKS);
    actor.setOnFire(true);
    owner.syncActorFlags(actor);
}
