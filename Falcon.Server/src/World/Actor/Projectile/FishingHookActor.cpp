#include "World/Actor/Projectile/FishingHookActor.h"

#include "Core/Math/MathConstants.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/Storage/Loot/LootItems.h"
#include "World/Level/Storage/Loot/LootTable.h"
#include "World/Level/Storage/Loot/LootTableRegistry.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Types/EntityDataMap.h"

#include <algorithm>
#include <cmath>
#include <random>

FALCON_REGISTER_ACTOR(FishingHookActor, FishingHookActor::IDENTIFIER);

namespace {
    const char *const FISHING_ROD = "minecraft:fishing_rod";
    const char *const FISHING_LOOT = "gameplay/fishing.json";
    const char *const JUNGLE_FISHING_LOOT = "gameplay/jungle_fishing.json";
    const char *const JUNGLE_TAG = "jungle";

    // Client-side particles for the fish swimming towards the hook, the bite, and a fish teasing the hook.
    const EntityEventType FISH_HOOK_BUBBLE = (EntityEventType) 12;
    const EntityEventType FISH_HOOK_HOOK = (EntityEventType) 13;
    const EntityEventType FISH_HOOK_TEASE = (EntityEventType) 14;

    const int32_t FISH_X_DATA_ID = 33;
    const int32_t FISH_Z_DATA_ID = 34;
    const int32_t FISH_ANGLE_DATA_ID = 35;

    const float CAST_AIM_SPEED = 0.4f;
    const float CAST_POWER = 1.5f;
    const float CAST_SPREAD = 0.0075f;
    const float PLAYER_EYE_HEIGHT = 1.62f;

    const int32_t WATER_SLICES = 5;
    const float WATER_PULL = 0.04f;
    const float AIR_DRAG = 0.92f;
    const float FLOAT_DRAG = 0.9f;
    const float FLOAT_DAMPING = 0.8f;
    const float FLOAT_LIFT = 0.05f;
    const float FLOAT_MAX_RISE = 0.15f;
    const float SUBMERGED_DEPTH = 1.01f;
    const float BOB_STEP = 0.1f;
    const float BOB_AMPLITUDE = 0.035f;
    const float HIT_GROW = 0.3f;
    const float HOOKED_HEIGHT_RATIO = 0.8f;

    const float MAX_ANGLER_DISTANCE_SQUARED = 1024.0f;

    const float RAIN_BONUS_CHANCE = 0.25f;
    const float COVERED_PENALTY_CHANCE = 0.5f;
    const int32_t BITE_WINDOW_BASE = 10;
    const int32_t BITE_WINDOW_RANGE = 20;
    const float NIBBLE_JOLT = -0.5f;
    const float FISH_ANGLE_DRIFT = 1.0f / 512.0f;
    const float FISH_OFFSET_SCALE = 0.1f;
    const float FISH_ANGLE_RANGE = 360.0f;
    const float RADIANS_TO_DEGREES = 57.295776f;
    const int32_t APPROACH_BASE = 20;
    const int32_t APPROACH_RANGE = 60;
    const float TEASE_DISTANCE_BASE = 25.0f;
    const float TEASE_DISTANCE_RANGE = 35.0f;
    const float TEASE_CHANCE_BASE = 0.15f;
    const int32_t LURE_BASE_SECONDS = 5;
    const int32_t LURE_RANGE_SECONDS = 25;
    const int32_t LURE_SECONDS_PER_LEVEL = 5;
    const float SCHOOL_SPEEDUP = 0.25f;
    const float SCHOOL_RADIUS = 8.0f;
    const int32_t TICKS_PER_SECOND = 20;

    const int32_t DURABILITY_FISH = 1;
    const int32_t DURABILITY_IN_GROUND = 2;
    const int32_t DURABILITY_ACTOR = 3;
    const int32_t EXPERIENCE_RANGE = 5;

    const float PULL_STRENGTH = 0.1f;
    const float PULL_LIFT = 0.08f;

    // Actors with the WaterAnimal actor type (0x2300) in 1.26.52, which make fish bite sooner.
    const char *const WATER_ANIMALS[] = {
            "minecraft:squid", "minecraft:glow_squid", "minecraft:dolphin", "minecraft:cod", "minecraft:salmon",
            "minecraft:pufferfish", "minecraft:tropicalfish", "minecraft:tadpole", "minecraft:nautilus"
    };

    std::mt19937 &hookRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float randomFloat() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(hookRandom());
    }

    int32_t randomInt(int32_t bound) {
        return std::uniform_int_distribution<int32_t>(0, bound - 1)(hookRandom());
    }

    float randomGaussian() {
        return std::normal_distribution<float>(0.0f, 1.0f)(hookRandom());
    }

    bool isWaterAnimal(const std::string &identifier) {
        for (const char *const animal: WATER_ANIMALS) {
            if (identifier == animal)
                return true;
        }
        return false;
    }

    bool containsPoint(const Vector3f &feet, float width, float height, const Vector3f &point) {
        const float halfWidth = width * 0.5f + HIT_GROW;
        return point.x >= feet.x - halfWidth && point.x <= feet.x + halfWidth
               && point.y >= feet.y - HIT_GROW && point.y <= feet.y + height + HIT_GROW
               && point.z >= feet.z - halfWidth && point.z <= feet.z + halfWidth;
    }

    float distanceSquared(const Vector3f &a, const Vector3f &b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }
}

FishingHookActor::FishingHookActor(uint64_t runtimeId, const std::string &identifier)
        : ServerActor(runtimeId, identifier) {
}

void FishingHookActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    // The client draws the fishing line from the actor named in the owner data.
    EntityDataEntry entry;
    entry.mId = ActorFlags::OWNER_DATA_ID;
    entry.mFormat = EntityDataFormat::Long;
    entry.mLongValue = mAnglerUniqueId;
    metadata.mEntries.push_back(entry);
}

bool FishingHookActor::isOwnedBy(const ServerPlayer &player) const {
    return !mExpired && getOwnerUniqueId() == (int64_t) player.getRuntimeId();
}

void FishingHookActor::cast(ServerNetworkHandler &owner, ServerPlayer &angler) {
    (void) owner;
    setOwnerUniqueId((int64_t) angler.getRuntimeId());
    mAnglerUniqueId = angler.getUniqueId();

    const Vector3f &rotation = angler.getRotation();
    const float pitch = rotation.x * MathConstants::DEGREES_TO_RADIANS_F;
    const float yaw = rotation.y * MathConstants::DEGREES_TO_RADIANS_F;
    const Vector3f anglerMotion = angler.getMotion();

    Vector3f motion(-std::sin(yaw) * std::cos(pitch) * CAST_AIM_SPEED + anglerMotion.x,
                    -std::sin(pitch) * CAST_AIM_SPEED + anglerMotion.y,
                    std::cos(yaw) * std::cos(pitch) * CAST_AIM_SPEED + anglerMotion.z);

    const float length = std::sqrt(motion.x * motion.x + motion.y * motion.y + motion.z * motion.z);
    if (length > 0.0f) {
        motion.x = (motion.x / length + randomGaussian() * CAST_SPREAD) * CAST_POWER;
        motion.y = (motion.y / length + randomGaussian() * CAST_SPREAD) * CAST_POWER;
        motion.z = (motion.z / length + randomGaussian() * CAST_SPREAD) * CAST_POWER;
    }
    setMotion(motion);

    Vector3f position = angler.getPosition();
    position.y += PLAYER_EYE_HEIGHT;
    setPosition(position);
}

void FishingHookActor::tick(ServerNetworkHandler &owner) {
    if (mExpired)
        return;

    ServerPlayer *angler = _findAngler(owner);
    if (angler == nullptr || !_anglerKeepsHook(*angler)) {
        mExpired = true;
        return;
    }

    Level &level = owner.getLevelFor(*this);
    if (mHookedUniqueId != 0) {
        _tickHooked(owner);
        return;
    }

    if (mInGround)
        return;

    const Vector3f &position = getPosition();
    if (level.getLiquidInfo((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                            (int32_t) std::floor(position.z)).lava) {
        mExpired = true;
        return;
    }

    const float waterFraction = _waterFraction(level);
    _move(owner, level, waterFraction);

    if (!mInGround && mHookedUniqueId == 0 && waterFraction > 0.0f)
        _tickCatching(owner, level, *angler);

    owner.broadcastActorMove(*this);
}

ServerPlayer *FishingHookActor::_findAngler(ServerNetworkHandler &owner) const {
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if ((int64_t) player.getRuntimeId() == getOwnerUniqueId())
            return player.isSpawned() && !player.isDead() ? &player : nullptr;
    }
    return nullptr;
}

bool FishingHookActor::_anglerKeepsHook(const ServerPlayer &angler) const {
    const ItemStack &held = angler.getInventory().getItemInHand();
    return angler.sharesLevelWith(*this) && held.mDefinition != nullptr
           && held.mDefinition->getIdentifier() == FISHING_ROD
           && distanceSquared(getPosition(), angler.getPosition()) <= MAX_ANGLER_DISTANCE_SQUARED;
}

void FishingHookActor::_tickHooked(ServerNetworkHandler &owner) {
    ServerActor *hooked = owner.getActor(mHookedUniqueId);
    if (hooked == nullptr || !hooked->isAlive() || !hooked->sharesLevelWith(*this)) {
        _setHooked(owner, 0);
        return;
    }

    Vector3f position = hooked->getPosition();
    position.y += hooked->getSize().mHeight * HOOKED_HEIGHT_RATIO;
    setPosition(position);
    setMotion(hooked->getMotion());
    owner.broadcastActorMove(*this);
}

float FishingHookActor::_waterFraction(Level &level) const {
    const Vector3f &position = getPosition();
    const float sliceHeight = getSize().mHeight / (float) WATER_SLICES;
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t z = (int32_t) std::floor(position.z);

    float fraction = 0.0f;
    for (int32_t slice = 0; slice < WATER_SLICES; ++slice) {
        const int32_t y = (int32_t) std::floor(position.y + sliceHeight * ((float) slice + 0.5f));
        if (level.getLiquidInfo(x, y, z).water)
            fraction += 1.0f / (float) WATER_SLICES;
    }
    return fraction;
}

void FishingHookActor::_move(ServerNetworkHandler &owner, Level &level, float waterFraction) {
    mBobTime += BOB_STEP;

    const Vector3f previous = getPosition();
    const int32_t blockX = (int32_t) std::floor(previous.x);
    const int32_t blockY = (int32_t) std::floor(previous.y);
    const int32_t blockZ = (int32_t) std::floor(previous.z);

    float surfaceDepth = 0.0f;
    float bob = 0.0f;
    if (level.getLiquidInfo(blockX, blockY, blockZ).water) {
        if (level.getLiquidInfo(blockX, blockY + 1, blockZ).water) {
            surfaceDepth = SUBMERGED_DEPTH;
        } else {
            surfaceDepth = (float) blockY - previous.y + 1.0f;
            bob = (std::sin(mBobTime) + 1.0f) * BOB_AMPLITUDE;
        }
    }

    float drag = AIR_DRAG;
    Vector3f motion = getMotion();
    float vertical = motion.y + (waterFraction * 2.0f - 1.0f) * WATER_PULL;
    if (surfaceDepth != 0.0f) {
        drag *= FLOAT_DRAG;
        vertical = std::min(vertical * FLOAT_DAMPING + FLOAT_LIFT, (surfaceDepth - bob) * FLOAT_MAX_RISE);
    }
    motion = Vector3f(motion.x * drag, vertical * drag, motion.z * drag);

    const Vector3f next(previous.x + motion.x, previous.y + motion.y, previous.z + motion.z);
    if (level.isSolidAt((int32_t) std::floor(next.x), (int32_t) std::floor(next.y), (int32_t) std::floor(next.z))) {
        mInGround = true;
        setMotion(Vector3f(0.0f, 0.0f, 0.0f));
        return;
    }

    setMotion(motion);
    setPosition(next);
    if (waterFraction <= 0.0f)
        _tryHookActor(owner, previous, next);
}

void FishingHookActor::_tryHookActor(ServerNetworkHandler &owner, const Vector3f &from, const Vector3f &to) {
    for (auto &entry: owner.getActors()) {
        MobActor *mob = dynamic_cast<MobActor *>(entry.second.get());
        if (mob == nullptr || !mob->isAlive() || !mob->sharesLevelWith(*this))
            continue;

        const MobActor &candidate = *mob;
        const ActorSize size = candidate.getSize();
        if (containsPoint(candidate.getPosition(), size.mWidth, size.mHeight, to)
            || containsPoint(candidate.getPosition(), size.mWidth, size.mHeight, from)) {
            _setHooked(owner, candidate.getUniqueId());
            return;
        }
    }
}

void FishingHookActor::_setHooked(ServerNetworkHandler &owner, int64_t uniqueId) {
    mHookedUniqueId = uniqueId;

    EntityDataEntry entry;
    entry.mId = ActorFlags::TARGET_DATA_ID;
    entry.mFormat = EntityDataFormat::Long;
    entry.mLongValue = uniqueId;

    EntityDataMap metadata;
    metadata.mEntries.push_back(entry);
    owner.sendActorMetadata(*this, metadata);
}

void FishingHookActor::_sendFishData(ServerNetworkHandler &owner, float x, float z, const float *angle) {
    EntityDataMap metadata;
    const int32_t ids[] = {FISH_X_DATA_ID, FISH_Z_DATA_ID, FISH_ANGLE_DATA_ID};
    const float values[] = {x, z, angle == nullptr ? 0.0f : *angle};
    const size_t count = angle == nullptr ? 2 : 3;
    for (size_t index = 0; index < count; ++index) {
        EntityDataEntry entry;
        entry.mId = ids[index];
        entry.mFormat = EntityDataFormat::Float;
        entry.mFloatValue = values[index];
        metadata.mEntries.push_back(entry);
    }
    owner.sendActorMetadata(*this, metadata);
}

int32_t FishingHookActor::_catchRate(Level &level) const {
    const Vector3f &position = getPosition();
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t z = (int32_t) std::floor(position.z);
    const int32_t above = (int32_t) std::floor(position.y) + 1;
    const bool exposed = above >= level.getHeightAt(x, z);

    int32_t rate = 1;
    if (randomFloat() < RAIN_BONUS_CHANCE && level.isRaining() && exposed)
        rate++;
    if (randomFloat() < COVERED_PENALTY_CHANCE && !exposed)
        rate--;
    return rate;
}

void FishingHookActor::_tickCatching(ServerNetworkHandler &owner, Level &level, const ServerPlayer &angler) {
    if (mNibble > 0) {
        mNibble--;
        if (mNibble == 0)
            mTimeUntilHooked = 0;

        Vector3f motion = getMotion();
        motion.y += randomFloat() * randomFloat() * randomFloat() * NIBBLE_JOLT;
        setMotion(motion);
        return;
    }

    const int32_t rate = _catchRate(level);
    const Vector3f &position = getPosition();

    if (mTimeUntilHooked > 0) {
        mTimeUntilHooked -= rate;
        if (mTimeUntilHooked <= 0) {
            mNibble = randomInt(BITE_WINDOW_RANGE) + BITE_WINDOW_BASE;
            owner.broadcastActorEvent(*this, FISH_HOOK_HOOK);
            return;
        }

        mFishAngle += randomGaussian() * FISH_ANGLE_DRIFT;
        const float offsetX = std::sin(mFishAngle) * (float) mTimeUntilHooked * FISH_OFFSET_SCALE;
        const float offsetZ = std::cos(mFishAngle) * (float) mTimeUntilHooked * FISH_OFFSET_SCALE;
        const float angleDegrees = mFishAngle * RADIANS_TO_DEGREES;
        _sendFishData(owner, offsetX, offsetZ, &angleDegrees);

        if (level.getLiquidInfo((int32_t) std::floor(position.x + offsetX), (int32_t) std::floor(position.y),
                                (int32_t) std::floor(position.z + offsetZ)).water)
            owner.broadcastActorEvent(*this, FISH_HOOK_BUBBLE);
        return;
    }

    if (mTimeUntilLured <= 0) {
        _startLuring(owner, angler);
        return;
    }

    mTimeUntilLured -= rate;

    float teaseChance = TEASE_CHANCE_BASE;
    if (mTimeUntilLured < 20)
        teaseChance += (float) (20 - mTimeUntilLured) * 0.05f;
    else if (mTimeUntilLured < 40)
        teaseChance += (float) (40 - mTimeUntilLured) * 0.02f;
    else if (mTimeUntilLured < 60)
        teaseChance += (float) (60 - mTimeUntilLured) * 0.01f;

    if (randomFloat() < teaseChance) {
        const float angle = randomFloat() * FISH_ANGLE_RANGE;
        const float distance = randomFloat() * TEASE_DISTANCE_RANGE + TEASE_DISTANCE_BASE;
        // The binary writes the absolute tease position here, unlike the hook-relative offset while approaching.
        _sendFishData(owner, position.x + distance * std::sin(angle) * FISH_OFFSET_SCALE,
                      position.z + distance * std::cos(angle) * FISH_OFFSET_SCALE, nullptr);
        owner.broadcastActorEvent(*this, FISH_HOOK_TEASE);
    }

    if (mTimeUntilLured <= 0) {
        mFishAngle = randomFloat() * FISH_ANGLE_RANGE;
        mTimeUntilHooked = randomInt(APPROACH_RANGE) + APPROACH_BASE;
    }
}

void FishingHookActor::_startLuring(ServerNetworkHandler &owner, const ServerPlayer &angler) {
    const int32_t lure = ItemEnchantments::getLevel(angler.getInventory().getItemInHand(), EnchantmentIds::LURE);
    int32_t seconds = randomInt(LURE_RANGE_SECONDS) + LURE_BASE_SECONDS - LURE_SECONDS_PER_LEVEL * lure;

    const Vector3f &position = getPosition();
    for (auto &entry: owner.getActors()) {
        const ServerActor &candidate = *entry.second;
        if (!candidate.isAlive() || !candidate.sharesLevelWith(*this) || !isWaterAnimal(candidate.getTypeId()))
            continue;

        const Vector3f &other = candidate.getPosition();
        if (std::fabs(other.x - position.x) <= SCHOOL_RADIUS && std::fabs(other.y - position.y) <= SCHOOL_RADIUS
            && std::fabs(other.z - position.z) <= SCHOOL_RADIUS) {
            seconds = (int32_t) std::floor((float) seconds * SCHOOL_SPEEDUP);
            break;
        }
    }

    mTimeUntilLured = std::max(1, seconds) * TICKS_PER_SECOND;
}

int32_t FishingHookActor::retrieve(ServerNetworkHandler &owner, ServerPlayer &angler) {
    if (mExpired)
        return 0;

    int32_t durability = 0;
    if (mHookedUniqueId != 0) {
        ServerActor *hooked = owner.getActor(mHookedUniqueId);
        if (hooked != nullptr && hooked->isAlive()) {
            _pullTowards(owner, *hooked, angler);
            durability = DURABILITY_ACTOR;
        }
    } else if (mNibble > 0) {
        _catch(owner, angler);
        durability = DURABILITY_FISH;
    }

    if (mInGround)
        durability = DURABILITY_IN_GROUND;

    mExpired = true;
    return durability;
}

void FishingHookActor::_catch(ServerNetworkHandler &owner, ServerPlayer &angler) {
    Level &level = owner.getLevelFor(*this);
    const Vector3f &position = getPosition();
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y);
    const int32_t z = (int32_t) std::floor(position.z);
    LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
    const bool jungle = chunk != nullptr
                        && BiomeChunkGenDataRegistry::hasTag((int32_t) chunk->getBiomeAt(x & 15, y, z & 15), JUNGLE_TAG);
    const LootTable *table = LootTableRegistry::getInstance().get(jungle ? JUNGLE_FISHING_LOOT : FISHING_LOOT);
    if (table == nullptr)
        return;

    LootContext context(hookRandom());
    context.mDifficulty = (int32_t) owner.getProperties().getDifficulty();
    context.mLuck = (float) ItemEnchantments::getLevel(angler.getInventory().getItemInHand(),
                                                       EnchantmentIds::LUCK_OF_THE_SEA);

    const Vector3f &target = angler.getPosition();
    const float dx = (target.x - position.x) * PULL_STRENGTH;
    const float dy = (target.y + 1.0f - position.y) * PULL_STRENGTH;
    const float dz = (target.z - position.z) * PULL_STRENGTH;
    const Vector3f motion(dx, dy + std::sqrt(std::sqrt(dx * dx + dy * dy + dz * dz)) * PULL_LIFT, dz);

    for (const LootDrop &drop: table->roll(context)) {
        const ItemStack stack = LootItems::toItemStack(owner, drop);
        if (!stack.isAir())
            owner.dropItem(level, position, stack, motion, ItemActorHandler::DROP_PICKUP_DELAY);
    }

    owner.spawnExperienceOrbs(level, target, randomInt(EXPERIENCE_RANGE) + 1);
}

void FishingHookActor::_pullTowards(ServerNetworkHandler &owner, Actor &target, const ServerPlayer &angler) const {
    const Vector3f &from = target.getPosition();
    const Vector3f &to = angler.getPosition();
    const float dx = (to.x - from.x) * PULL_STRENGTH;
    const float dy = (to.y + 1.0f - from.y) * PULL_STRENGTH;
    const float dz = (to.z - from.z) * PULL_STRENGTH;

    target.setMotion(Vector3f(dx, dy + std::sqrt(std::sqrt(dx * dx + dy * dy + dz * dz)) * PULL_LIFT, dz));
    owner.sendActorMotion(target);
}
