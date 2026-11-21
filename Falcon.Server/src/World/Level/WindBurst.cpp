#include "World/Level/WindBurst.h"

#include "Core/Json/Json.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Explosion.h"
#include "World/Level/GameRules.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {
    const int32_t RAYS = 16;
    const float STEP = 0.3f;
    const float STEP_LOSS = 0.22500001f;
    const float BLOCK_LOSS = 0.3f;
    const float RESISTANCE_SCALE = 5.0f;
    // how far a burst reaches against its power
    const float REACH_PER_POWER = 2.0f;
    const float MOB_EYE_RATIO = 0.85f;
    const char *const AIR = "minecraft:air";
    const char *const LIT = "lit";
    const char *const EXTINGUISH_SOUND = "extinguish.candle";

    float numberOf(const json::Value &object, const char *key, float fallback) {
        const json::Value *value = object.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    bool flagOf(const json::Value &object, const char *key, bool fallback) {
        const json::Value *value = object.get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }

    bool isWater(const std::string &name) {
        return name == "minecraft:water" || name == "minecraft:flowing_water";
    }

    bool isLiquid(const std::string &name) {
        return isWater(name) || name == "minecraft:lava" || name == "minecraft:flowing_lava";
    }

    bool endsWith(const std::string &value, const char *suffix) {
        const size_t length = std::char_traits<char>::length(suffix);
        return value.size() >= length && value.compare(value.size() - length, length, suffix) == 0;
    }

    // lit candles and candle cakes go out
    bool extinguish(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state) {
        if (!endsWith(state.mName, "candle") && !endsWith(state.mName, "candle_cake"))
            return false;
        if (!state.mStates.contains(LIT) || state.mStates.getByte(LIT, 0) == 0)
            return false;

        Tag states = state.mStates;
        states.putByte(LIT, 0);
        const BlockState out(state.mName, states);
        level.setBlock(position, out, true);
        BlockActionHandler::broadcastBlockUpdate(owner, level, position, out);
        owner.playLevelSound(level, EXTINGUISH_SOUND,
                             Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f));
        return true;
    }

    AxisAlignedBB boxOf(const Vector3f &feet, float width, float height) {
        const float half = width * 0.5f;
        return AxisAlignedBB(feet.x - half, feet.y, feet.z - half, feet.x + half, feet.y + height, feet.z + half);
    }

    float knockbackKept(const Actor &actor) {
        const float resistance =
                actor.getAttributes().get("minecraft:knockback_resistance") + actor.getArmorKnockbackResistance();
        return std::max(1.0f - resistance, 0.0f);
    }

    // toward the eyes; primed tnt is pushed from its feet
    Vector3f pushDirection(const Vector3f &center, const Vector3f &feet, float aimHeight) {
        const Vector3f to(feet.x - center.x, feet.y + aimHeight - center.y, feet.z - center.z);
        const float length = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
        if (length < 0.0001f)
            return Vector3f();
        return Vector3f(to.x / length, to.y / length, to.z / length);
    }
}

WindBurst::Settings WindBurst::fromComponent(const json::Value &component) {
    Settings settings;
    settings.mPower = numberOf(component, "power", settings.mPower);
    settings.mKnockbackScaling = numberOf(component, "knockback_scaling", settings.mKnockbackScaling);
    settings.mMaxResistance = numberOf(component, "max_resistance", 3.4028235e38f);
    settings.mTogglesBlocks = flagOf(component, "toggles_blocks", false);
    settings.mNegatesFallDamage = flagOf(component, "negates_fall_damage", false);
    settings.mAllowUnderwater = flagOf(component, "allow_underwater", false);
    return settings;
}

float WindBurst::impact(float distance, float exposure, const Settings &settings) {
    const float reach = settings.mPower * REACH_PER_POWER;
    if (reach <= 0.0f)
        return 0.0f;
    const float scaled = distance / reach;
    if (scaled > 1.0f)
        return 0.0f;
    return (1.0f - scaled) * exposure * settings.mKnockbackScaling;
}

std::vector<Vector3i> WindBurst::reachedBlocks(Level &level, const Vector3f &center, const Settings &settings,
                                               std::mt19937 &random) {
    std::vector<Vector3i> reached;
    std::unordered_set<int64_t> seen;
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float cap = settings.mMaxResistance / RESISTANCE_SCALE;
    const int32_t last = RAYS - 1;

    // one ray per surface point of a 16x16x16 cube
    for (int32_t i = 0; i < RAYS; ++i) {
        for (int32_t j = 0; j < RAYS; ++j) {
            for (int32_t k = 0; k < RAYS; ++k) {
                if (i != 0 && i != last && j != 0 && j != last && k != 0 && k != last)
                    continue;

                float force = (unit(random) * 0.6f + 0.7f) * settings.mPower;
                const float x = (float) i / 15.0f * 2.0f - 1.0f;
                const float y = (float) j / 15.0f * 2.0f - 1.0f;
                const float z = (float) k / 15.0f * 2.0f - 1.0f;
                const float length = std::sqrt(x * x + y * y + z * z);
                const Vector3f stride(x / length * STEP, y / length * STEP, z / length * STEP);
                Vector3f at = center;
                while (force > 0.0f) {
                    const Vector3i block((int32_t) std::floor(at.x), (int32_t) std::floor(at.y),
                                         (int32_t) std::floor(at.z));
                    if (block.y < LevelChunk::MIN_Y || block.y > LevelChunk::MAX_Y)
                        break;

                    const BlockState *state = level.peekBlockPtr(block.x, block.y, block.z);
                    if (state != nullptr && !settings.mAllowUnderwater && isLiquid(state->mName))
                        force = 0.0f;
                    const bool passable = state == nullptr || state->mName == AIR
                                          || (settings.mAllowUnderwater && isWater(state->mName));
                    if (!passable) {
                        const BlockData *data = BlockDataTable::find(state->mName.c_str());
                        const float resistance = std::min(data == nullptr ? 0.0f : data->mResistance / RESISTANCE_SCALE,
                                                          cap);
                        force -= (resistance + BLOCK_LOSS) * STEP;
                        if (force > 0.0f) {
                            const int64_t key = ((int64_t) block.x & 0x3FFFFFF) | (((int64_t) block.z & 0x3FFFFFF) << 26)
                                                | (((int64_t) block.y & 0xFFF) << 52);
                            if (seen.insert(key).second)
                                reached.push_back(block);
                        }
                    }
                    at = Vector3f(at.x + stride.x, at.y + stride.y, at.z + stride.z);
                    force -= STEP_LOSS;
                }
            }
        }
    }
    return reached;
}

void WindBurst::burst(ServerNetworkHandler &owner, Level &level, const Vector3f &center, const Settings &settings,
                      const ServerActor *source) {
    const float reach = settings.mPower * REACH_PER_POWER;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.isSpectator() || &owner.getLevelFor(player) != &level)
            continue;

        const Vector3f feet = player.getPosition();
        const Vector3f offset(feet.x - center.x, feet.y - center.y, feet.z - center.z);
        const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
        if (distance > reach)
            continue;

        const float exposure = Explosion::getBlockDensity(level, center, boxOf(feet, 0.6f, 1.8f));
        const float push = impact(distance, exposure, settings) * knockbackKept(player);
        if (push <= 0.0f)
            continue;

        const Vector3f direction = pushDirection(center, feet, MovementPhysics::PLAYER_EYE_HEIGHT);
        const Vector3f current = player.hasSimulatedVelocity() ? player.getSimulatedVelocity() : player.getMotion();
        player.applyServerMotion(owner, Vector3f(current.x + direction.x * push, current.y + direction.y * push,
                                                 current.z + direction.z * push));
        if (settings.mNegatesFallDamage)
            player.setWindImpulseHeight(feet.y);
    }

    for (auto &entry: owner.getActors()) {
        ServerActor &actor = *entry.second;
        if (&actor == source || !actor.isAlive() || &owner.getLevelFor(actor) != &level)
            continue;

        const ActorSize size = actor.getSize();
        const Vector3f feet = actor.getPosition();
        const Vector3f offset(feet.x - center.x, feet.y - center.y, feet.z - center.z);
        // measured from the side of the box, not its middle
        const float distance =
                std::max(0.0f, std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z)
                                       - size.mWidth * 0.5f);
        if (distance > reach)
            continue;

        const float exposure = Explosion::getBlockDensity(level, center, boxOf(feet, size.mWidth, size.mHeight));
        const float push = impact(distance, exposure, settings) * knockbackKept(actor);
        if (push <= 0.0f)
            continue;

        const bool tnt = actor.getIdentifier() == "minecraft:tnt";
        const Vector3f direction = pushDirection(center, feet, tnt ? 0.0f : size.mHeight * MOB_EYE_RATIO);
        const Vector3f motion = actor.getMotion();
        actor.setMotion(Vector3f(motion.x + direction.x * push, motion.y + direction.y * push,
                                 motion.z + direction.z * push));
    }

    if (!settings.mTogglesBlocks)
        return;
    if (settings.mNeedsMobGriefing && !level.getGameRules().getBool("mobgriefing"))
        return;

    static std::mt19937 random(std::random_device{}());
    for (const Vector3i &position: reachedBlocks(level, center, settings, random)) {
        const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
        if (state == nullptr)
            continue;
        const BlockState copy = *state;
        if (extinguish(owner, level, position, copy))
            continue;
        if (const Block *block = VanillaBlocks::fromIdentifier(copy.mName))
            block->onWindBurst(owner, level, position, copy);
    }
}
