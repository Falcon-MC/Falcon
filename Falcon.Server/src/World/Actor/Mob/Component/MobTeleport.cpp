#include "World/Actor/Mob/Component/MobTeleport.h"

#include "Core/Math/Vector3i.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    const char *const COMPONENT = "minecraft:teleport";
    const float TICKS_PER_SECOND = 20.0f;
    // the definition's defaults
    const bool DEFAULT_RANDOM_TELEPORTS = true;
    const float DEFAULT_MIN_TIME = 0.0f;
    const float DEFAULT_MAX_TIME = 20.0f;
    const Vector3f DEFAULT_CUBE(32.0f, 16.0f, 32.0f);
    const float DEFAULT_TARGET_DISTANCE = 16.0f;
    const float DEFAULT_TARGET_CHANCE = 1.0f;
    const float DEFAULT_LIGHT_CHANCE = 0.01f;
    const float DEFAULT_DARK_CHANCE = 0.01f;
    // brighter than this counts as daylight, when the sky is in view
    const float DAYLIGHT_BRIGHTNESS = 0.5f;
    const float MAX_LIGHT = 15.0f;
    // tries at a spot away from a projectile; a random teleport has one
    const int32_t DODGE_ATTEMPTS = 64;
    // toward a target: a step of 16 back along the line, spread over 8 blocks across and 16 up and down
    const float TOWARD_STEP = 16.0f;
    const float TOWARD_SPREAD = 8.0f;
    const int32_t TOWARD_HEIGHT_SPREAD = 16;
    const float EYE_HEIGHT_RATIO = 0.85f;
    // LevelEvent of the enderman teleport sound
    const int32_t LEVEL_EVENT_TELEPORT_SOUND = 1018;
    // families the game leaves out when a definition has no projectile_filter
    const char *const UNDODGED_FAMILIES[] = {"splash_potion", "lingering_potion", "snowball", "xp_bottle"};

    std::mt19937 &teleportRandom() {
        static std::mt19937 generator{std::random_device{}()};
        return generator;
    }

    float nextFloat() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(teleportRandom());
    }

    float numberOf(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    bool flagOf(const json::Value &component, const char *key, bool fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }

    Vector3f cubeOf(const json::Value &component) {
        const json::Value *cube = component.get("random_teleport_cube");
        if (cube == nullptr || cube->mArray.size() < 3)
            return DEFAULT_CUBE;
        return Vector3f((float) cube->mArray[0]->number(DEFAULT_CUBE.x),
                        (float) cube->mArray[1]->number(DEFAULT_CUBE.y),
                        (float) cube->mArray[2]->number(DEFAULT_CUBE.z));
    }

    int32_t ticksOf(const json::Value &component, const char *key, float fallback) {
        return (int32_t) (numberOf(component, key, fallback) * TICKS_PER_SECOND);
    }

    float brightnessAt(Level &level, const Vector3i &block) {
        const int32_t sky = level.hasSkyLight()
                                    ? level.getSkyLightAt(block.x, block.y, block.z) - level.getSkyLightSubtracted()
                                    : 0;
        const int32_t light = std::max(sky, level.getBlockLightAt(block.x, block.y, block.z));
        return (float) std::max(0, light) / MAX_LIGHT;
    }

    bool hasLiquid(Level &level, const AxisAlignedBB &box) {
        for (int32_t x = (int32_t) std::floor(box.mMinX); x <= (int32_t) std::floor(box.mMaxX); ++x) {
            for (int32_t y = (int32_t) std::floor(box.mMinY); y <= (int32_t) std::floor(box.mMaxY); ++y) {
                for (int32_t z = (int32_t) std::floor(box.mMinZ); z <= (int32_t) std::floor(box.mMaxZ); ++z) {
                    const LiquidInfo liquid = level.getLiquidInfo(x, y, z);
                    if (liquid.water || liquid.lava)
                        return true;
                }
            }
        }
        return false;
    }

    // the game drops the spot onto the first block below with a collision, then wants the box free and dry there
    bool teleportTo(ServerNetworkHandler &owner, MobActor &mob, const Vector3f &target) {
        Level &level = owner.getLevelFor(mob);
        const int32_t x = (int32_t) std::floor(target.x);
        int32_t y = (int32_t) std::floor(target.y);
        const int32_t z = (int32_t) std::floor(target.z);
        if (y <= level.getMinY())
            return false;

        while (true) {
            const BlockState *below = level.peekBlockPtr(x, y - 1, z);
            if (below == nullptr)
                return false;
            if (BlockShape::hasCollision(*below, &mob))
                break;
            if (--y <= level.getMinY())
                return false;
        }

        const Vector3f from = mob.getPosition();
        const Vector3f destination(target.x, (float) y, target.z);
        const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(mob).offset(
                destination.x - from.x, destination.y - from.y, destination.z - from.z);
        if (!level.getCollisionBoxes(box, &mob).empty() || hasLiquid(level, box))
            return false;

        mob.getNavigation().stop(mob);
        mob.teleport(destination);
        owner.broadcastActorMove(mob, true);
        level.postGameEvent(GameEvents::TELEPORT, from, &mob);
        if (!mob.getFlags().get(ActorFlag::Silent)) {
            owner.broadcastLevelEvent(level, LEVEL_EVENT_TELEPORT_SOUND, from, 0);
            owner.broadcastLevelEvent(level, LEVEL_EVENT_TELEPORT_SOUND, destination, 0);
        }
        return true;
    }

    Vector3f randomSpot(const MobActor &mob, const Vector3f &cube) {
        const Vector3f position = mob.getPosition();
        const float x = (nextFloat() - 0.5f) * cube.x;
        const float y = (nextFloat() - 0.5f) * cube.y;
        const float z = (nextFloat() - 0.5f) * cube.z;
        return Vector3f(position.x + x, position.y + y, position.z + z);
    }

    bool dodges(ServerNetworkHandler &owner, const MobActor &mob, const json::Value &component,
                const ServerActor &projectile) {
        const json::Value *filter = component.get("projectile_filter");
        if (filter != nullptr)
            return EntityFilter::test(*filter, owner, mob, &projectile);
        for (const char *family: UNDODGED_FAMILIES) {
            if (EntityFilter::hasFamily(projectile, family))
                return false;
        }
        return true;
    }

    float eyeHeightOf(const Actor &actor) {
        if (const auto *player = dynamic_cast<const ServerPlayer *>(&actor))
            return player->getEyeHeight();
        if (const auto *server = dynamic_cast<const ServerActor *>(&actor))
            return server->getSize().mHeight * EYE_HEIGHT_RATIO;
        return 0.0f;
    }
}

// daylight under open sky, else darkness, may send it off; a far target pulls it closer; else the timer runs
void MobTeleport::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(COMPONENT);
    if (component == nullptr || !mob.isAlive())
        return;

    const float lightChance = numberOf(*component, "light_teleport_chance", DEFAULT_LIGHT_CHANCE);
    const float darkChance = numberOf(*component, "dark_teleport_chance", DEFAULT_DARK_CHANCE);
    if (lightChance > 0.0f || darkChance > 0.0f) {
        Level &level = owner.getLevelFor(mob);
        const Vector3f position = mob.getPosition();
        const Vector3i block((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                             (int32_t) std::floor(position.z));
        const bool bright = brightnessAt(level, block) > DAYLIGHT_BRIGHTNESS;
        const bool underSky = block.y >= level.getHeightAt(block.x, block.z);
        // bright but under a roof: neither chance
        if (!bright || underSky) {
            const float chance = bright ? lightChance : darkChance;
            if (chance > 0.0f && chance > nextFloat()) {
                randomTeleport(owner, mob);
                return;
            }
        }
    }

    if (Actor *target = mob.getTarget(owner)) {
        const Vector3f from = mob.getPosition();
        const Vector3f to = target->getPosition();
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float dz = to.z - from.z;
        const float distance = numberOf(*component, "target_distance", DEFAULT_TARGET_DISTANCE);
        if (dx * dx + dy * dy + dz * dz > distance * distance
            && numberOf(*component, "target_teleport_chance", DEFAULT_TARGET_CHANCE) > nextFloat())
            _teleportTowards(owner, mob, Vector3f(to.x, to.y + eyeHeightOf(*target), to.z));
        return;
    }

    if (!flagOf(*component, "random_teleports", DEFAULT_RANDOM_TELEPORTS))
        return;

    if (mCountdown > 1) {
        --mCountdown;
        return;
    }

    const int32_t minimum = ticksOf(*component, "min_random_teleport_time", DEFAULT_MIN_TIME);
    const int32_t maximum = ticksOf(*component, "max_random_teleport_time", DEFAULT_MAX_TIME);
    mCountdown = minimum;
    if (maximum > minimum)
        mCountdown += std::uniform_int_distribution<int32_t>(0, maximum - minimum - 1)(teleportRandom());
    randomTeleport(owner, mob);
}

bool MobTeleport::dodge(ServerNetworkHandler &owner, MobActor &mob, const ServerActor &projectile) {
    const json::Value *component = mob.getComponent(COMPONENT);
    if (component == nullptr || !flagOf(*component, "teleports_on_projectile_hit", false)
        || !dodges(owner, mob, *component, projectile))
        return false;

    const Vector3f cube = cubeOf(*component);
    for (int32_t attempt = 0; attempt < DODGE_ATTEMPTS; ++attempt) {
        if (teleportTo(owner, mob, randomSpot(mob, cube)))
            return true;
    }
    return false;
}

bool MobTeleport::randomTeleport(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(COMPONENT);
    if (component == nullptr)
        return false;
    return teleportTo(owner, mob, randomSpot(mob, cubeOf(*component)));
}

// the eye of the target, seen from the middle of the mob
bool MobTeleport::_teleportTowards(ServerNetworkHandler &owner, MobActor &mob, const Vector3f &target) {
    const Vector3f position = mob.getPosition();
    float dx = position.x - target.x;
    float dy = position.y + mob.getSize().mHeight * 0.5f - target.y;
    float dz = position.z - target.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length >= 0.0001f) {
        dx = dx / length * TOWARD_STEP;
        dy = dy / length * TOWARD_STEP;
        dz = dz / length * TOWARD_STEP;
    } else {
        dx = dy = dz = 0.0f;
    }

    const float x = position.x + (nextFloat() - 0.5f) * TOWARD_SPREAD - dx;
    const int32_t height = std::uniform_int_distribution<int32_t>(0, TOWARD_HEIGHT_SPREAD - 1)(teleportRandom());
    const float y = position.y + (float) (height - TOWARD_HEIGHT_SPREAD / 2) - dy;
    const float z = position.z + (nextFloat() - 0.5f) * TOWARD_SPREAD - dz;
    return teleportTo(owner, mob, Vector3f(x, y, z));
}
