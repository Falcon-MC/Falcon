#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/DynamicPropertyStore.h"
#include "Actor/ActorSizeTable.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/Mob/MobLoot.h"
#include "Actor/ServerActor.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Math/MathConstants.h"
#include "Protocol/Packets/AddActorPacket.h"
#include "Protocol/Packets/AnimateEntityPacket.h"
#include "Protocol/Packets/MoveActorAbsolutePacket.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/RemoveActorPacket.h"
#include "Protocol/Packets/SetActorDataPacket.h"
#include "Protocol/Packets/SetActorMotionPacket.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/SpawnParticleEffectPacket.h"
#include "Protocol/Packets/CameraInstructionPacket.h"
#include "Protocol/Packets/MobEffectPacket.h"
#include "Protocol/Packets/PlayerStartItemCooldownPacket.h"
#include "Protocol/Packets/RemoveObjectivePacket.h"
#include "Protocol/Packets/SetDisplayObjectivePacket.h"
#include "Protocol/Packets/SetPlayerGameTypePacket.h"
#include "Protocol/Packets/SetScorePacket.h"
#include "Protocol/Packets/SetTitlePacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "Item/Item.h"
#include "Item/ItemData.h"
#include "Item/ItemEnchantments.h"
#include "Item/EnchantmentData.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Item/Items/FireworkRocketItem.h"
#include "Item/Items/RangedWeaponItems.h"
#include "Item/Items/ThrowableItems.h"
#include "Item/PotionEffects.h"
#include "Item/StringToItemParser.h"
#include "Actor/ExperienceValues.h"
#include "Protocol/Types/StartGameTypes.h"

#include <random>
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Scripting/Content/CustomContentRegistry.h"

#include <algorithm>
#include "Network/Handler/ChunkStreamHandler.h"

#include <cmath>

namespace {
    const float ACTOR_GRAVITY = 0.05f;
    const float ARROW_GRAVITY = 0.05f;
    const float ARROW_KNOCKBACK = 0.3f;
    const float PUNCH_KNOCKBACK_PER_LEVEL = 0.5f;
    const float IMPALING_DAMAGE_PER_LEVEL = 2.5f;
    const float TRIDENT_RETURN_SPEED = 0.6f;
    const float TRIDENT_RETURN_REACH = 1.5f;
    const float FIREWORK_HORIZONTAL_ACCELERATION = 1.15f;
    const float FIREWORK_VERTICAL_ACCELERATION = 0.04f;
    const int32_t FIREWORK_ITEM_DATA_ID = 16;
    const int32_t PROJECTILE_MAX_LIFETIME = 1200;
    const float WIND_CHARGE_KNOCKBACK_STRENGTH = 0.2f;
    const float WIND_CHARGE_LIFT = 0.6f;
    const int32_t ACTOR_MAX_DEATH_TICKS = 25;
    const float ACTOR_SUFFOCATION_DAMAGE = 1.0f;
    const int32_t LINGERING_CLOUD_WAIT_TIME = 10;
    const int32_t LINGERING_CLOUD_DURATION = 600;
    const int32_t LINGERING_CLOUD_APPLY_INTERVAL = 10;
    const float LINGERING_CLOUD_RADIUS_PER_TICK = -0.005f;
    const float LINGERING_CLOUD_RADIUS_ON_USE = -0.5f;
    const float LINGERING_CLOUD_MIN_RADIUS = 1.5f;
    const float PLAYER_WIDTH = 0.6f;
    const float PLAYER_HEIGHT = 1.8f;
    const float PLAYER_EYE_HEIGHT = 1.62f;
    const float PROJECTILE_HIT_GROW = 0.3f;
    const int32_t EXPERIENCE_ORB_PICKUP_DELAY = 10;
    const int32_t EXPERIENCE_ORB_MAX_AGE = 6000;
    const float EXPERIENCE_ORB_ATTRACT_RANGE_SQUARED = 64.0f;
    const float EXPERIENCE_ORB_GRAVITY = 0.04f;
    const float EXPERIENCE_ORB_DRAG = 0.02f;
    const float EXPERIENCE_ORB_GROUND_FRICTION = 0.6f;
    const float EXPERIENCE_ORB_PICKUP_REACH = 1.0f;
    const int32_t ACTOR_DATA_SCALE = 38;
    const int32_t EGG_HATCH_CHANCE = 8;
    const int32_t EGG_QUADRUPLE_HATCH_CHANCE = 32;
    const int32_t EGG_QUADRUPLE_HATCH_COUNT = 4;
    const float EGG_HATCH_HEIGHT = 0.5f;
    const float BABY_SCALE = 0.5f;

    bool intersectsActorBox(const Vector3f &actorPosition, float width, float height, const Vector3f &point) {
        const float halfWidth = width * 0.5f + PROJECTILE_HIT_GROW;

        const float minX = actorPosition.x - halfWidth;
        const float maxX = actorPosition.x + halfWidth;
        const float minY = actorPosition.y - PROJECTILE_HIT_GROW;
        const float maxY = actorPosition.y + height + PROJECTILE_HIT_GROW;
        const float minZ = actorPosition.z - halfWidth;
        const float maxZ = actorPosition.z + halfWidth;

        return point.x >= minX && point.x <= maxX && point.y >= minY && point.y <= maxY &&
               point.z >= minZ && point.z <= maxZ;
    }

    EntityProperties buildActorProperties(ServerActor &actor) {
        EntityProperties properties;

        const CustomActorDefinition *definition = actor.getDefinition();
        if (definition == nullptr)
            return properties;

        for (const ActorPropertyDescription &descriptor: definition->mProperties) {
            if (descriptor.mType == ActorPropertyDescription::Type::Float) {
                FloatEntityProperty property;
                property.mIndex = descriptor.mIndex;
                property.mValue = actor.getFloatProperty(descriptor.mName, descriptor.mDefaultFloat);
                properties.mFloatProperties.push_back(property);
            } else {
                IntEntityProperty property;
                property.mIndex = descriptor.mIndex;
                property.mValue = actor.getIntProperty(descriptor.mName, descriptor.mDefaultInt);
                properties.mIntProperties.push_back(property);
            }
        }

        return properties;
    }
}

ServerActor *ServerNetworkHandler::spawnActor(Level &level, const std::string &identifier, const Vector3f &position) {
    const uint64_t runtimeId = allocateRuntimeId();
    const int64_t uniqueId = (int64_t) runtimeId;

    std::unique_ptr<ServerActor> actor = ActorClassRegistry::create(runtimeId, identifier);
    actor->getAttributes() = ActorAttributes::createActorDefaults();
    actor->setDimension(level.getDimensionType());
    actor->setPosition(position);
    actor->resetFallDistance();

    MobActor *mob = dynamic_cast<MobActor *>(actor.get());
    if (mob != nullptr)
        mob->applyDefaults(mProperties.getDifficulty());

    const CustomActorDefinition *definition = CustomContentRegistry::getInstance().getActorDefinition(identifier);
    if (definition != nullptr) {
        actor->setDefinition(definition);
        actor->setProjectile(definition->mIsProjectile);

        for (const ActorPropertyDescription &descriptor: definition->mProperties) {
            if (descriptor.mType == ActorPropertyDescription::Type::Float)
                actor->setFloatProperty(descriptor.mName, descriptor.mDefaultFloat);
            else
                actor->setIntProperty(descriptor.mName, descriptor.mDefaultInt);
        }
    }

    ServerActor *result = actor.get();
    mActors[uniqueId] = std::move(actor);

    broadcastActorSpawn(*result);
    return result;
}

FallingBlockActor *ServerNetworkHandler::spawnFallingBlock(Level &level, const BlockState &state,
                                                           const Vector3f &position) {
    const uint64_t runtimeId = allocateRuntimeId();
    const int64_t uniqueId = (int64_t) runtimeId;

    std::unique_ptr<FallingBlockActor> actor(new FallingBlockActor(runtimeId, state));
    actor->getAttributes() = ActorAttributes::createActorDefaults();
    actor->setDimension(level.getDimensionType());
    actor->setPosition(position);
    actor->setHighestPosition(position.y);

    FallingBlockActor *result = actor.get();
    mActors[uniqueId] = std::move(actor);

    broadcastActorSpawn(*result);
    return result;
}

PrimedTntActor *ServerNetworkHandler::spawnPrimedTnt(Level &level, const Vector3f &position, const Vector3f &motion,
                                                     int32_t fuse) {
    const uint64_t runtimeId = allocateRuntimeId();
    const int64_t uniqueId = (int64_t) runtimeId;

    std::unique_ptr<PrimedTntActor> actor(new PrimedTntActor(runtimeId, fuse));
    actor->getAttributes() = ActorAttributes::createActorDefaults();
    actor->setDimension(level.getDimensionType());
    actor->setPosition(position);
    actor->setMotion(motion);

    PrimedTntActor *result = actor.get();
    mActors[uniqueId] = std::move(actor);

    broadcastActorSpawn(*result);
    return result;
}

void ServerNetworkHandler::spawnExperienceOrbs(Level &level, const Vector3f &position, int amount) {
    if (amount <= 0)
        return;

    static std::mt19937 orbRandom(0x1F123BB5u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    for (int value: ExperienceValues::splitIntoOrbSizes(amount)) {
        ServerActor *orb = spawnActor(level, "minecraft:xp_orb", position);
        if (orb == nullptr)
            continue;

        orb->setExperienceValue(value);
        orb->setPickupDelay(EXPERIENCE_ORB_PICKUP_DELAY);
        orb->setMotion(Vector3f((unit(orbRandom) * 0.2f - 0.1f) * 2.0f,
                                unit(orbRandom) * 0.4f,
                                (unit(orbRandom) * 0.2f - 0.1f) * 2.0f));
    }
}

bool ServerNetworkHandler::tickExperienceOrb(ServerActor &orb) {
    orb.decrementPickupDelay();

    if (orb.getLifetimeTicks() > EXPERIENCE_ORB_MAX_AGE)
        return true;

    Vector3f motion = orb.getMotion();
    Vector3f position = orb.getPosition();

    ServerPlayer *closest = nullptr;
    float closestDistanceSquared = EXPERIENCE_ORB_ATTRACT_RANGE_SQUARED;
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead())
            continue;
        if (player.getGameType() == (int32_t) GameType::Spectator)
            continue;
        if (player.getDimension() != orb.getDimension())
            continue;

        const Vector3f playerPosition = player.getPosition();
        const float dx = playerPosition.x - position.x;
        const float dy = playerPosition.y - position.y;
        const float dz = playerPosition.z - position.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared < closestDistanceSquared) {
            closest = &player;
            closestDistanceSquared = distanceSquared;
        }
    }

    if (closest != nullptr && orb.getPickupDelay() <= 0) {
        const Vector3f playerPosition = closest->getPosition();
        const float dx = std::fabs(playerPosition.x - position.x);
        const float dz = std::fabs(playerPosition.z - position.z);

        if (dx <= EXPERIENCE_ORB_PICKUP_REACH && dz <= EXPERIENCE_ORB_PICKUP_REACH &&
            position.y >= playerPosition.y - EXPERIENCE_ORB_PICKUP_REACH &&
            position.y <= playerPosition.y + PLAYER_HEIGHT) {
            closest->getExperience().addXp(orb.getExperienceValue());
            closest->syncExperience();
            _sendAttributes(*closest);
            playNamedSound(getLevelFor(orb), "random.orb", position, 0.1f, 1.0f);
            return true;
        }
    }

    if (closest != nullptr) {
        const Vector3f playerPosition = closest->getPosition();
        const float dX = (playerPosition.x - position.x) / 8.0f;
        const float dY = (playerPosition.y + PLAYER_EYE_HEIGHT * 0.5f - position.y) / 8.0f;
        const float dZ = (playerPosition.z - position.z) / 8.0f;
        const float distance = std::sqrt(dX * dX + dY * dY + dZ * dZ);
        float diff = 1.0f - distance;

        if (diff > 0.0f && distance > 0.0f) {
            diff = diff * diff;
            motion.x += dX / distance * diff * 0.1f;
            motion.y += dY / distance * diff * 0.1f;
            motion.z += dZ / distance * diff * 0.1f;
        }
    }

    motion.y -= EXPERIENCE_ORB_GRAVITY;

    Vector3f next(position.x + motion.x, position.y + motion.y, position.z + motion.z);

    const int32_t blockX = (int32_t) std::floor(next.x);
    const int32_t blockZ = (int32_t) std::floor(next.z);
    const int32_t blockY = (int32_t) std::floor(next.y);

    bool onGround = false;
    if (motion.y < 0.0f && getLevelFor(orb).isSolidAt(blockX, blockY, blockZ)) {
        next.y = (float) (blockY + 1);
        onGround = true;
    }

    float friction = 1.0f - EXPERIENCE_ORB_DRAG;
    if (onGround)
        friction *= EXPERIENCE_ORB_GROUND_FRICTION;

    motion.x *= friction;
    motion.y *= 1.0f - EXPERIENCE_ORB_DRAG;
    motion.z *= friction;

    if (onGround)
        motion.y *= -0.5f;

    orb.setMotion(motion);
    orb.setPosition(next);
    orb.setOnGround(onGround);

    broadcastActorMove(orb);
    return false;
}

ServerActor *ServerNetworkHandler::getActor(int64_t uniqueId) {
    auto it = mActors.find(uniqueId);
    return it == mActors.end() ? nullptr : it->second.get();
}

ServerActor *ServerNetworkHandler::spawnProjectile(ServerPlayer &player, const std::string &identifier, float speed,
                                                   float verticalOffset) {
    const Vector3f &rotation = player.getRotation();
    const float pitch = rotation.x * MathConstants::DEGREES_TO_RADIANS_F;
    const float yaw = rotation.y * MathConstants::DEGREES_TO_RADIANS_F;

    const Vector3f direction(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));

    Vector3f spawnPosition = player.getPosition();
    spawnPosition.y += PLAYER_EYE_HEIGHT + verticalOffset;

    ServerActor *projectile = spawnActor(getLevelFor(player), identifier, spawnPosition);
    if (projectile == nullptr)
        return nullptr;

    projectile->setProjectile(true);
    projectile->setOwnerUniqueId((int64_t) player.getRuntimeId());
    projectile->getProjectileData().mLootingLevel =
            ItemEnchantments::getLevel(player.getInventory().getItemInHand(), EnchantmentIds::LOOTING);
    projectile->setMotion(Vector3f(direction.x * speed, direction.y * speed, direction.z * speed));
    return projectile;
}

bool ServerNetworkHandler::onThrownProjectileHit(ServerActor &projectile, const Vector3f &hitPosition,
                                                 ServerPlayer *hitPlayer) {
    const std::string identifier = projectile.getIdentifier();
    Level &level = getLevelFor(projectile);

    if (isArrowProjectile(identifier)) {
        if (hitPlayer != nullptr)
            return onArrowProjectileHitTarget(projectile, hitPosition, *hitPlayer);

        ProjectileData &data = projectile.getProjectileData();
        const bool isTrident = identifier == "minecraft:thrown_trident";
        playLevelSound(level, isTrident ? LevelSoundEvent::TRIDENT_HIT_GROUND : LevelSoundEvent::BOW_HIT,
                       hitPosition);

        if (isTrident && data.mLoyaltyLevel > 0) {
            data.mReturning = true;
            playLevelSound(level, LevelSoundEvent::TRIDENT_RETURN, hitPosition);
            return false;
        }

        dropProjectileItem(projectile, hitPosition);
        return true;
    }

    if (identifier == "minecraft:ender_pearl") {
        const int64_t ownerId = projectile.getOwnerUniqueId();
        for (auto &entry: mPlayers) {
            ServerPlayer &shooter = entry.second;
            if ((int64_t) shooter.getRuntimeId() != ownerId)
                continue;

            shooter.teleport(*this, hitPosition, MovePlayerTeleportationCause::Behavior);
            applyDamage(shooter, 5.0f, "death.fell.accident.generic", {shooter.getName()}, false, false);
            break;
        }
        spawnParticleEffect(level, "minecraft:endermanpop_emitter", hitPosition);
        return true;
    }

    if (identifier == "minecraft:snowball") {
        if (hitPlayer != nullptr) {
            const Vector3f target = hitPlayer->getPosition();
            knockBack(*hitPlayer, target.x - hitPosition.x, target.z - hitPosition.z, 0.3f);
        }
        spawnParticleEffect(level, "minecraft:snowballpoof", hitPosition);
        return true;
    }

    if (identifier == "minecraft:egg") {
        _hatchEggChicks(level, hitPosition);
        return true;
    }

    if (identifier == "minecraft:wind_charge_projectile") {
        const float burstRadius = 3.5f;
        for (auto &entry: mPlayers) {
            ServerPlayer &nearby = entry.second;
            if (!nearby.isSpawned() || nearby.getDimension() != projectile.getDimension())
                continue;

            const Vector3f position = nearby.getPosition();
            const float dx = position.x - hitPosition.x;
            const float dy = position.y - hitPosition.y;
            const float dz = position.z - hitPosition.z;
            const float distanceSquared = dx * dx + dy * dy + dz * dz;
            if (distanceSquared > burstRadius * burstRadius)
                continue;

            pushFrom(nearby, hitPosition, WIND_CHARGE_KNOCKBACK_STRENGTH, WIND_CHARGE_LIFT);
        }

        for (auto &entry: mActors) {
            ServerActor &nearby = *entry.second;
            if (!nearby.isAlive() || nearby.isProjectile() || nearby.getDimension() != projectile.getDimension())
                continue;

            const Vector3f position = nearby.getPosition();
            const float dx = position.x - hitPosition.x;
            const float dy = position.y - hitPosition.y;
            const float dz = position.z - hitPosition.z;
            const float distanceSquared = dx * dx + dy * dy + dz * dz;
            if (distanceSquared > burstRadius * burstRadius)
                continue;

            pushFrom(nearby, hitPosition, WIND_CHARGE_KNOCKBACK_STRENGTH, WIND_CHARGE_LIFT);
        }

        const Vector3f burstPosition(hitPosition.x, hitPosition.y + 1.0f, hitPosition.z);
        spawnParticleEffect(level, "minecraft:wind_explosion_emitter", burstPosition);
        playLevelSound(level, LevelSoundEvent::WIND_CHARGE_BURST, burstPosition);
        return true;
    }

    if (identifier == "minecraft:xp_bottle") {
        static std::mt19937 bottleRandom(0x3A5F19C7u);
        std::uniform_int_distribution<int> amount(3, 11);

        spawnExperienceOrbs(level, hitPosition, amount(bottleRandom));

        LevelEventPacket splash;
        splash.mEventId = LevelEventPacket::ParticleSplash;
        splash.mPosition = hitPosition;
        splash.mData = 0x00385dc6;

        for (auto &entry: mPlayers) {
            if (entry.second.isSpawned() && entry.second.getDimension() == level.getDimensionType())
                mNetworkHandler->send(entry.first, splash, mCodecContext);
        }

        playLevelSound(level, LevelSoundEvent::GLASS, hitPosition);
        return true;
    }

    if (identifier == "minecraft:splash_potion") {
        const auto it = mProjectilePotionId.find(projectile.getUniqueId());
        const int32_t potionId = it != mProjectilePotionId.end() ? it->second : 0;
        if (it != mProjectilePotionId.end())
            mProjectilePotionId.erase(it);

        for (auto &entry: mPlayers) {
            ServerPlayer &nearby = entry.second;
            if (!nearby.isSpawned() || nearby.getDimension() != projectile.getDimension())
                continue;

            const Vector3f position = nearby.getPosition();
            const float dx = position.x - hitPosition.x;
            const float dy = position.y - hitPosition.y;
            const float dz = position.z - hitPosition.z;
            const float distanceSquared = dx * dx + dy * dy + dz * dz;
            if (distanceSquared > 16.0f)
                continue;

            const float scale = std::max(0.25f, 1.0f - std::sqrt(distanceSquared) / 4.0f);
            applyPotionEffects(nearby, potionId, scale);
        }

        spawnParticleEffect(level, "minecraft:splash_spell_emitter", hitPosition);
        return true;
    }

    if (identifier == "minecraft:lingering_potion") {
        const auto it = mProjectilePotionId.find(projectile.getUniqueId());
        const int32_t potionId = it != mProjectilePotionId.end() ? it->second : 0;
        if (it != mProjectilePotionId.end())
            mProjectilePotionId.erase(it);

        ServerActor *cloud = spawnActor(level, "minecraft:area_effect_cloud", hitPosition);
        if (cloud != nullptr) {
            const float cloudRadius = 3.0f;

            EntityDataMap metadata;

            EntityDataEntry radius;
            radius.mId = ACTOR_DATA_AREA_EFFECT_CLOUD_RADIUS;
            radius.mFormat = EntityDataFormat::Float;
            radius.mFloatValue = cloudRadius;
            metadata.mEntries.push_back(radius);

            EntityDataEntry particle;
            particle.mId = ACTOR_DATA_AREA_EFFECT_CLOUD_PARTICLE_ID;
            particle.mFormat = EntityDataFormat::Int;
            particle.mIntValue = AREA_EFFECT_CLOUD_POTION_PARTICLE;
            metadata.mEntries.push_back(particle);

            EntityDataEntry auxValue;
            auxValue.mId = ACTOR_DATA_POTION_AUX_VALUE;
            auxValue.mFormat = EntityDataFormat::Short;
            auxValue.mShortValue = (int16_t) potionId;
            metadata.mEntries.push_back(auxValue);

            EntityDataEntry color;
            color.mId = ACTOR_DATA_POTION_COLOR;
            color.mFormat = EntityDataFormat::Int;
            color.mIntValue = getPotionColor(potionId);
            metadata.mEntries.push_back(color);

            EntityDataEntry width;
            width.mId = ACTOR_DATA_WIDTH;
            width.mFormat = EntityDataFormat::Float;
            width.mFloatValue = cloudRadius;
            metadata.mEntries.push_back(width);

            EntityDataEntry height;
            height.mId = ACTOR_DATA_HEIGHT;
            height.mFormat = EntityDataFormat::Float;
            height.mFloatValue = 0.5f;
            metadata.mEntries.push_back(height);

            sendActorMetadata(*cloud, metadata);

            LingeringCloud state;
            state.mPotionId = potionId;
            state.mAge = 0;
            state.mWaitTime = LINGERING_CLOUD_WAIT_TIME;
            state.mDuration = LINGERING_CLOUD_DURATION;
            state.mNextApply = 0;
            state.mReapplicationDelay = 0;
            state.mRadius = cloudRadius;
            state.mRadiusPerTick = LINGERING_CLOUD_RADIUS_PER_TICK;
            state.mRadiusOnUse = LINGERING_CLOUD_RADIUS_ON_USE;
            state.mPosition = hitPosition;
            state.mDimension = cloud->getDimension();
            mLingeringClouds[cloud->getUniqueId()] = state;
        }
        return true;
    }

    return isThrownProjectile(identifier);
}

void ServerNetworkHandler::dropProjectileItem(ServerActor &projectile, const Vector3f &position) {
    const ProjectileData &data = projectile.getProjectileData();
    if (data.mPickupItem.isAir())
        return;

    ItemActorHandler::dropItem(*this, getLevelFor(projectile), position, data.mPickupItem,
                               ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
}

void ServerNetworkHandler::returnProjectileToOwner(ServerPlayer &player, ServerActor &projectile) {
    const ProjectileData &data = projectile.getProjectileData();
    if (data.mPickupItem.isAir())
        return;

    PlayerInventory &inventory = player.getInventory();
    const int slot = data.mFavoredSlot;
    if (slot >= 0 && slot < PlayerInventory::CONTAINER_SIZE && inventory.getItem(slot).isAir()) {
        inventory.setItem(slot, data.mPickupItem);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        return;
    }

    std::vector<int> touched;
    if (!inventory.addItem(data.mPickupItem, touched)) {
        dropProjectileItem(projectile, player.getPosition());
        return;
    }

    for (int touchedSlot: touched)
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, touchedSlot);
}

float ServerNetworkHandler::computeProjectileDamage(ServerActor &projectile) {
    const ProjectileData &data = projectile.getProjectileData();
    const Vector3f motion = projectile.getMotion();
    const float speed = std::sqrt(motion.x * motion.x + motion.y * motion.y + motion.z * motion.z);

    int32_t damage = (int32_t) std::ceil(speed * data.mBaseDamage);
    if (data.mCritical && damage > 0) {
        static std::mt19937 generator{std::random_device{}()};
        std::uniform_int_distribution<int32_t> bonus(0, damage / 2 + 1);
        damage += bonus(generator);
    }

    return (float) damage;
}

bool ServerNetworkHandler::onArrowProjectileHitTarget(ServerActor &projectile, const Vector3f &hitPosition,
                                                      Actor &target) {
    ProjectileData &data = projectile.getProjectileData();
    const bool isTrident = std::string(projectile.getIdentifier()) == "minecraft:thrown_trident";

    float damage = computeProjectileDamage(projectile);
    if (isTrident && data.mImpalingLevel > 0 && LiquidBlocksFetch::at(getLevelFor(target), target.getPosition()).water)
        damage += IMPALING_DAMAGE_PER_LEVEL * (float) data.mImpalingLevel;

    ServerPlayer *shooter = nullptr;
    for (auto &entry: mPlayers) {
        if ((int64_t) entry.second.getRuntimeId() == projectile.getOwnerUniqueId())
            shooter = &entry.second;
    }

    ServerPlayer *victimPlayer = dynamic_cast<ServerPlayer *>(&target);
    if (victimPlayer != nullptr) {
        applyDamage(*victimPlayer, damage, "death.attack.arrow",
                    {victimPlayer->getName(), shooter == nullptr ? std::string() : shooter->getName()},
                    false, false);
    } else {
        ServerActor *victimActor = dynamic_cast<ServerActor *>(&target);
        if (victimActor != nullptr)
            damageActor(*victimActor, damage, shooter, data.mLootingLevel);
    }

    const Vector3f targetPosition = target.getPosition();
    const float knockback = ARROW_KNOCKBACK + PUNCH_KNOCKBACK_PER_LEVEL * (float) data.mPunchLevel;
    knockBack(target, targetPosition.x - hitPosition.x, targetPosition.z - hitPosition.z, knockback);

    if (data.mFlameTicks > 0 && !target.hasEffect(MobEffectId::FireResistance)) {
        target.setFireTicks(data.mFlameTicks);
        target.setOnFire(true);

        ServerActor *burningActor = dynamic_cast<ServerActor *>(&target);
        if (burningActor != nullptr)
            syncActorFlags(*burningActor);
        else if (victimPlayer != nullptr)
            _sendEntityData(*victimPlayer);
    }

    Level &level = getLevelFor(projectile);
    playLevelSound(level, isTrident ? LevelSoundEvent::TRIDENT_HIT : LevelSoundEvent::BOW_HIT, hitPosition);

    if (isTrident) {
        data.mHadCollision = true;

        if (data.mChanneling && level.hasSkyLight() && mLevel.isThundering())
            strikeLightning(level, targetPosition);

        if (data.mLoyaltyLevel > 0 && shooter != nullptr) {
            data.mReturning = true;
            playLevelSound(level, LevelSoundEvent::TRIDENT_RETURN, hitPosition);
            return false;
        }

        dropProjectileItem(projectile, hitPosition);
        return true;
    }

    if (data.mPiercingLevel > 0) {
        data.mPiercingLevel -= 1;
        return false;
    }

    return true;
}

bool ServerNetworkHandler::onThrownProjectileHitActor(ServerActor &projectile, const Vector3f &hitPosition,
                                                      ServerActor &hitActor) {
    const std::string identifier = projectile.getIdentifier();

    if (isArrowProjectile(identifier))
        return onArrowProjectileHitTarget(projectile, hitPosition, hitActor);

    if (hitActor.isProjectile()) {
        const bool handled = onThrownProjectileHit(projectile, hitPosition, nullptr);
        const bool otherHandled = onThrownProjectileHit(hitActor, hitActor.getPosition(), nullptr);
        removeActor(hitActor.getUniqueId());
        return handled || otherHandled;
    }

    if (identifier == "minecraft:snowball") {
        const float snowballDamage = std::string(hitActor.getIdentifier()) == "minecraft:blaze" ? 3.0f : 0.0f;
        damageActor(hitActor, snowballDamage, nullptr);
        knockBack(hitActor, hitActor.getPosition().x - hitPosition.x, hitActor.getPosition().z - hitPosition.z, 0.3f);
        spawnParticleEffect(getLevelFor(projectile), "minecraft:snowballpoof", hitPosition);
        return true;
    }

    if (identifier == "minecraft:egg") {
        damageActor(hitActor, 0.0f, nullptr);
        knockBack(hitActor, hitActor.getPosition().x - hitPosition.x, hitActor.getPosition().z - hitPosition.z, 0.2f);
        return true;
    }

    return onThrownProjectileHit(projectile, hitPosition, nullptr);
}

void ServerNetworkHandler::setProjectilePotionData(int64_t uniqueId, int32_t potionId) {
    mProjectilePotionId[uniqueId] = potionId;
}

void ServerNetworkHandler::applyPotionEffects(ServerPlayer &player, int32_t potionId, float durationScale) {
    for (const PotionEffect &effect: getPotionEffects(potionId)) {
        if (effect.mInstant) {
            if (effect.mId == MobEffectId::InstantHealth) {
                player.heal(4.0f * (float) (1 << effect.mAmplifier));
            } else if (effect.mId == MobEffectId::InstantDamage) {
                applyDamage(player, 6.0f * (float) (1 << effect.mAmplifier), "death.attack.magic",
                            {player.getName()}, false, false);
            }
            continue;
        }

        MobEffectInstance instance;
        instance.mId = effect.mId;
        instance.mDuration = (int32_t) ((float) effect.mDuration * durationScale);
        if (instance.mDuration < 1)
            instance.mDuration = 1;
        instance.mAmplifier = effect.mAmplifier;
        instance.mParticles = true;
        player.addEffect(instance);
    }
}

void ServerNetworkHandler::removeActor(int64_t uniqueId) {
    auto it = mActors.find(uniqueId);
    if (it == mActors.end())
        return;

    broadcastActorRemove(*it->second);
    mActors.erase(it);
}

bool ServerNetworkHandler::canPlayerSeeActor(ServerPlayer &player, const ServerActor &actor) const {
    if (!player.isSpawned() || player.getDimension() != actor.getDimension())
        return false;

    const Vector3f position = actor.getPosition();
    const int64_t hash = ChunkStreamHandler::packChunk((int32_t) std::floor(position.x) >> 4,
                                                       (int32_t) std::floor(position.z) >> 4);

    return player.getSentChunks().find(hash) != player.getSentChunks().end();
}

void ServerNetworkHandler::_sendActorSpawn(ServerPlayer &player, ServerActor &actor) {
    AddActorPacket packet;
    packet.mUniqueActorId = actor.getUniqueId();
    packet.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    packet.mIdentifier = actor.getTypeId();
    packet.mPosition = actor.getPosition();
    packet.mPosition.y += actor.getBaseOffset();
    packet.mMotion = actor.getMotion();
    packet.mRotation = Vector2f(actor.getRotation().x, actor.getRotation().y);
    packet.mProperties = buildActorProperties(actor);
    actor.fillSpawnMetadata(packet.mMetadata);

    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);
}

void ServerNetworkHandler::_sendActorRemove(ServerPlayer &player, const ServerActor &actor) {
    RemoveActorPacket packet;
    packet.mUniqueActorId = actor.getUniqueId();

    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);
}

void ServerNetworkHandler::refreshContainerViewers(const Vector3i &position, const ServerPlayer *except) {
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (&player == except)
            continue;

        if (player.isSpawned())
            player.getInventoryManager().refreshOpenContainer(position);
    }
}

void ServerNetworkHandler::broadcastActorSpawn(ServerActor &actor) {
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!canPlayerSeeActor(player, actor))
            continue;

        if (!player.getVisibleActors().insert(actor.getRuntimeId()).second)
            continue;

        _sendActorSpawn(player, actor);
    }
}

void ServerNetworkHandler::updateActorVisibility() {
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned())
            continue;

        std::unordered_set<uint64_t> &visible = player.getVisibleActors();

        for (auto &actorEntry: mActors) {
            ServerActor &actor = *actorEntry.second;
            const bool shouldSee = actor.isAlive() && canPlayerSeeActor(player, actor);
            const bool seen = visible.find(actor.getRuntimeId()) != visible.end();

            if (shouldSee == seen)
                continue;

            if (shouldSee) {
                visible.insert(actor.getRuntimeId());
                _sendActorSpawn(player, actor);
            } else {
                visible.erase(actor.getRuntimeId());
                _sendActorRemove(player, actor);
            }
        }
    }
}

void ServerNetworkHandler::sendActorsTo(ServerPlayer &player) {
    for (auto &entry: mActors) {
        ServerActor &actor = *entry.second;
        if (!actor.isAlive() || !canPlayerSeeActor(player, actor))
            continue;

        if (!player.getVisibleActors().insert(actor.getRuntimeId()).second)
            continue;

        _sendActorSpawn(player, actor);
    }
}

void ServerNetworkHandler::broadcastActorRemove(ServerActor &actor) {
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (player.getVisibleActors().erase(actor.getRuntimeId()) == 0)
            continue;

        _sendActorRemove(player, actor);
    }
}

void ServerNetworkHandler::syncActorFlags(ServerActor &actor) {
    SetActorDataPacket packet;
    packet.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    packet.mTick = 0;

    EntityDataEntry flags;
    flags.mId = ActorFlags::FLAGS_DATA_ID;
    flags.mFormat = EntityDataFormat::Long;
    flags.mLongValue = actor.getFlags().getLowBits();
    packet.mMetadata.mEntries.push_back(flags);

    EntityDataEntry flags2;
    flags2.mId = ActorFlags::FLAGS_2_DATA_ID;
    flags2.mFormat = EntityDataFormat::Long;
    flags2.mLongValue = actor.getFlags().getHighBits();
    packet.mMetadata.mEntries.push_back(flags2);

    packet.mProperties = buildActorProperties(actor);

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::syncActorFirework(ServerActor &actor) {
    SetActorDataPacket packet;
    packet.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    packet.mTick = 0;

    EntityDataEntry firework;
    firework.mId = FIREWORK_ITEM_DATA_ID;
    firework.mFormat = EntityDataFormat::Nbt;
    firework.mNbtValue = actor.getProjectileData().mFireworkData;
    packet.mMetadata.mEntries.push_back(firework);

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::syncActorProperties(ServerActor &actor) {
    SetActorDataPacket packet;
    packet.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    packet.mProperties = buildActorProperties(actor);
    packet.mTick = (int64_t) mCurrentTick;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::_hatchEggChicks(Level &level, const Vector3f &hitPosition) {
    static std::mt19937 hatchRandom(std::random_device{}());

    if (std::uniform_int_distribution<int32_t>(0, EGG_HATCH_CHANCE - 1)(hatchRandom) != 0)
        return;

    int32_t chicks = 1;
    if (std::uniform_int_distribution<int32_t>(0, EGG_QUADRUPLE_HATCH_CHANCE - 1)(hatchRandom) == 0)
        chicks = EGG_QUADRUPLE_HATCH_COUNT;

    const Vector3f spawnPosition(hitPosition.x, hitPosition.y + EGG_HATCH_HEIGHT, hitPosition.z);

    for (int32_t chick = 0; chick < chicks; ++chick) {
        ServerActor *hatched = spawnActor(level, "minecraft:chicken", spawnPosition);
        if (hatched == nullptr)
            continue;

        hatched->getFlags().set(ActorFlag::Baby, true);

        EntityDataMap metadata;

        EntityDataEntry flags;
        flags.mId = ActorFlags::FLAGS_DATA_ID;
        flags.mFormat = EntityDataFormat::Long;
        flags.mLongValue = hatched->getFlags().getLowBits();
        metadata.mEntries.push_back(flags);

        EntityDataEntry flags2;
        flags2.mId = ActorFlags::FLAGS_2_DATA_ID;
        flags2.mFormat = EntityDataFormat::Long;
        flags2.mLongValue = hatched->getFlags().getHighBits();
        metadata.mEntries.push_back(flags2);

        EntityDataEntry scale;
        scale.mId = ACTOR_DATA_SCALE;
        scale.mFormat = EntityDataFormat::Float;
        scale.mFloatValue = BABY_SCALE;
        metadata.mEntries.push_back(scale);

        sendActorMetadata(*hatched, metadata);
    }
}

void ServerNetworkHandler::sendActorNameTag(ServerActor &actor) {
    const int32_t ENTITY_DATA_NAME = 4;

    EntityDataEntry name;
    name.mId = ENTITY_DATA_NAME;
    name.mFormat = EntityDataFormat::String;
    name.mStringValue = actor.getNameTag();

    EntityDataMap metadata;
    metadata.mEntries.push_back(name);

    sendActorMetadata(actor, metadata);
}

void ServerNetworkHandler::sendActorMetadata(ServerActor &actor, const EntityDataMap &metadata) {
    SetActorDataPacket packet;
    packet.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    packet.mMetadata = metadata;
    packet.mTick = (int64_t) mCurrentTick;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::sendActorMotion(Actor &actor) {
    SetActorMotionPacket packet;
    packet.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    packet.mMotion = actor.getMotion();
    packet.mTick = (uint64_t) mCurrentTick;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::knockBack(Actor &actor, float deltaX, float deltaZ, float force, float verticalLimit) {
    actor.knockBack(deltaX, deltaZ, force, verticalLimit);
    sendActorMotion(actor);
}

void ServerNetworkHandler::pushFrom(Actor &actor, const Vector3f &origin, float strength, float lift) {
    const Vector3f position = actor.getPosition();

    Vector3f motion = actor.getMotion();
    motion.x = motion.x * 0.5f - (origin.x - position.x) * strength;
    motion.y = motion.y * 0.5f + lift;
    motion.z = motion.z * 0.5f - (origin.z - position.z) * strength;

    actor.setMotion(motion);
    sendActorMotion(actor);
}

void ServerNetworkHandler::broadcastActorEvent(ServerActor &actor, EntityEventType eventType) {
    ActorEventPacket packet;
    packet.mRuntimeActorId = actor.getRuntimeId();
    packet.mEventId = (uint8_t) eventType;
    packet.mEventData = 0;
    packet.mHasFirePosition = false;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

bool ServerNetworkHandler::damageActor(ServerActor &actor, float amount, ServerPlayer *source, int32_t lootingLevel) {
    return actor.hurt(*this, amount, source, lootingLevel);
}

void ServerNetworkHandler::broadcastActorMove(ServerActor &actor) {
    MoveActorAbsolutePacket move;
    move.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    move.mPosition = actor.getPosition();
    move.mPosition.y += actor.getBaseOffset();
    move.mRotation = actor.getRotation();

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, move, mCodecContext);
    }
}

void ServerNetworkHandler::playActorAnimation(ServerActor &actor, const std::string &animation) {
    AnimateEntityPacket packet;
    packet.mAnimation = animation;
    packet.mRuntimeActorIds.push_back(actor.getRuntimeId());

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::spawnParticleEffect(Level &level, const std::string &identifier,
                                               const Vector3f &position) {
    SpawnParticleEffectPacket packet;
    packet.mDimensionId = level.getDimensionId();
    packet.mUniqueActorId = -1;
    packet.mPosition = position;
    packet.mIdentifier = identifier;
    packet.mHasMolangVariablesJson = false;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned() && entry.second.getDimension() == level.getDimensionType())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::playLevelSound(Level &level, const std::string &sound, const Vector3f &position,
                                          const std::string &actorType, int32_t extraData) {
    LevelSoundEventPacket packet;
    packet.mSound = sound;
    packet.mPosition = position;
    packet.mExtraData = extraData;
    packet.mActorType = actorType;
    packet.mIsBabyMob = false;
    packet.mDisableRelativeVolume = false;
    packet.mActorUniqueId = -1;
    packet.mHasFirePosition = false;

    BlockActionHandler::broadcastToViewers(*this, level, position, packet);
}

void ServerNetworkHandler::playNamedSound(Level &level, const std::string &sound, const Vector3f &position,
                                          float volume, float pitch) {
    PlaySoundPacket packet;
    packet.mSound = sound;
    packet.mPosition = position;
    packet.mVolume = volume;
    packet.mPitch = pitch;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned() && entry.second.getDimension() == level.getDimensionType())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::spawnItemActor(Level &level, const std::string &typeId, int32_t amount,
                                          const Vector3f &position) {
    Item item;
    if (!StringToItemParser::getInstance().parse(typeId, item))
        return;

    std::shared_ptr<ItemDefinition> definition = mItemDefinitions.getDefinition(item.getIdentifier());
    if (definition == nullptr)
        return;

    ItemStack stack;
    stack.mDefinition = definition;
    stack.mBlockDefinition = mBlockDefinitions.getDefinition(item.getIdentifier());
    stack.mCount = amount < 1 ? 1 : amount;

    ItemActorHandler::dropItem(*this, level, position, stack, ItemActorHandler::randomDropMotion(),
                               ItemActorHandler::DROP_PICKUP_DELAY);
}

void ServerNetworkHandler::sendActionBar(ServerPlayer &player, const std::string &text, bool json) {
    player.sendActionBar(text, json);
}

void ServerNetworkHandler::sendTitle(ServerPlayer &player, const std::string &text, bool json) {
    player.sendTitleText(text, json);
}

void ServerNetworkHandler::applyActorEffect(ServerActor &actor, int32_t effectId, int32_t amplifier,
                                            int32_t durationTicks, bool particles) {
    MobEffectPacket packet;
    packet.mRuntimeActorId = actor.getRuntimeId();
    packet.mEvent = MobEffectPacket::Event::Add;
    packet.mEffectId = effectId;
    packet.mAmplifier = amplifier;
    packet.mParticles = particles;
    packet.mDuration = durationTicks;
    packet.mTick = (uint64_t) mCurrentTick;
    packet.mAmbient = false;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::playSoundFor(ServerPlayer &player, const std::string &sound, const Vector3f &position,
                                        float volume, float pitch) {
    PlaySoundPacket packet;
    packet.mSound = sound;
    packet.mPosition = position;
    packet.mVolume = volume;
    packet.mPitch = pitch;
    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);
}


void ServerNetworkHandler::clearPlayerCamera(ServerPlayer &player) {
    CameraInstructionPacket packet;
    packet.mHasClear = true;
    packet.mClear = true;
    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);
}

void ServerNetworkHandler::syncPlayerAttributes(ServerPlayer &player) {
    _sendAttributes(player);
}

void ServerNetworkHandler::displayScoreboardObjective(const std::string &slot, const std::string &objectiveId,
                                                      const std::string &displayName) {
    SetDisplayObjectivePacket packet;
    packet.mDisplaySlot = slot;
    packet.mObjectiveId = objectiveId;
    packet.mDisplayName = displayName.empty() ? objectiveId : displayName;
    packet.mCriteria = "dummy";
    packet.mSortOrder = 0;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::clearScoreboardDisplay(const std::string &slot) {
    SetDisplayObjectivePacket packet;
    packet.mDisplaySlot = slot;
    packet.mObjectiveId = "";
    packet.mDisplayName = "";
    packet.mCriteria = "dummy";

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::setScoreboardScore(const std::string &objectiveId, const std::string &participant,
                                              int32_t score) {
    const std::string key = objectiveId + "\x1f" + participant;
    auto it = mScoreboardIds.find(key);
    if (it == mScoreboardIds.end())
        it = mScoreboardIds.emplace(key, mNextScoreboardId++).first;

    ScoreInfoEntry info;
    info.mScoreboardId = it->second;
    info.mObjectiveId = objectiveId;
    info.mScore = score;
    info.mType = ScorerType::Fake;
    info.mName = participant;

    SetScorePacket packet;
    packet.mAction = SetScorePacket::Action::Change;
    packet.mInfos.push_back(info);

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::removeScoreboardScore(const std::string &objectiveId, const std::string &participant) {
    const std::string key = objectiveId + "\x1f" + participant;
    auto it = mScoreboardIds.find(key);
    if (it == mScoreboardIds.end())
        return;

    ScoreInfoEntry info;
    info.mScoreboardId = it->second;
    info.mObjectiveId = objectiveId;
    info.mType = ScorerType::Invalid;
    info.mName = participant;

    SetScorePacket packet;
    packet.mInfos.push_back(info);

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
    mScoreboardIds.erase(it);
}

void ServerNetworkHandler::removeScoreboardObjective(const std::string &objectiveId) {
    RemoveObjectivePacket packet;
    packet.mObjectiveId = objectiveId;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::sendJsonMessage(ServerPlayer &player, const std::string &json) {
    TextPacket packet;
    packet.mType = TextPacket::Type::Json;
    packet.mMessage = json;
    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);
}

void ServerNetworkHandler::playPlayerAnimation(ServerPlayer &player, const std::string &animation) {
    AnimateEntityPacket packet;
    packet.mAnimation = animation;
    packet.mRuntimeActorIds.push_back(player.getRuntimeId());

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}



void ServerNetworkHandler::loadWorldDynamicProperties() {
    Tag data;
    if (!mPlayerData.loadData("world_dynamic_properties", data))
        return;

    const Tag *properties = data.get("DynamicProperties");
    if (properties != nullptr)
        deserializeDynamicProperties(*properties, mWorldDynamicProperties);
}

void ServerNetworkHandler::saveWorldDynamicProperties() {
    Tag data = Tag::ofCompound();
    data.put("DynamicProperties", serializeDynamicProperties(mWorldDynamicProperties));
    mPlayerData.saveData("world_dynamic_properties", data);
}


void ServerNetworkHandler::tickActors() {
    std::vector<int64_t> expired;

    std::vector<int64_t> tickOrder;
    tickOrder.reserve(mActors.size());
    for (auto &entry: mActors)
        tickOrder.push_back(entry.first);

    for (const int64_t actorId: tickOrder) {
        auto actorEntry = mActors.find(actorId);
        if (actorEntry == mActors.end())
            continue;

        ServerActor &actor = *actorEntry->second;
        Level &level = getLevelFor(actor);
        actor.addLifetimeTick();
        actor.tickCombat(1);

        if (actor.isDead()) {
            actor.addDeathTick();
            if (actor.getDeathTicks() >= ACTOR_MAX_DEATH_TICKS)
                expired.push_back(actorId);
            continue;
        }

        if (std::string(actor.getIdentifier()) == "minecraft:xp_orb") {
            if (tickExperienceOrb(actor))
                expired.push_back(actorId);
            continue;
        }

        if (!actor.isProjectile()) {
            const ActorSize size = ActorSizeTable::getSize(actor.getIdentifier());
            if (_isEyeInsideSolidBlock(level, actor.getPosition(), size.mHeight))
                actor.hurt(*this, ACTOR_SUFFOCATION_DAMAGE, nullptr);
        }

        ProfilerScopedSection projectileSection(mProfiler, ProfilerSection::ActorProjectiles,
                                                actor.isProjectile());

        if (actor.isProjectile() && isFireworkRocketActor(actor.getIdentifier())) {
            ProjectileData &firework = actor.getProjectileData();

            ServerPlayer *rider = nullptr;
            if (firework.mFireworkAttached) {
                for (auto &playerEntry: mPlayers) {
                    if ((int64_t) playerEntry.second.getRuntimeId() == actor.getOwnerUniqueId() &&
                        playerEntry.second.isSpawned())
                        rider = &playerEntry.second;
                }
            }

            if (rider != nullptr) {
                actor.setPosition(rider->getPosition());
            } else {
                Vector3f motion = actor.getMotion();
                motion.x *= FIREWORK_HORIZONTAL_ACCELERATION;
                motion.z *= FIREWORK_HORIZONTAL_ACCELERATION;
                motion.y += FIREWORK_VERTICAL_ACCELERATION;

                Vector3f position = actor.getPosition();
                position.x += motion.x;
                position.y += motion.y;
                position.z += motion.z;

                actor.setMotion(motion);
                actor.setPosition(position);
            }

            broadcastActorMove(actor);
            actor.addLifetimeTick();
            ++firework.mFireworkAge;

            if (firework.mFireworkAge >= firework.mFireworkLifetime) {
                broadcastActorEvent(actor, EntityEventType::FireworkParticles);
                playLevelSound(level, LevelSoundEvent::LARGE_BLAST, actor.getPosition());

                if (rider != nullptr && rider->getFlags().get(ActorFlag::Gliding))
                    rider->getFlags().set(ActorFlag::Gliding, true);

                expired.push_back(actorId);
            }

            continue;
        }

        if (actor.isProjectile()) {
            Vector3f motion = actor.getMotion();
            const bool thrown = isThrownProjectile(actor.getIdentifier());
            const bool arrowLike = isArrowProjectile(actor.getIdentifier());

            if (arrowLike && actor.getProjectileData().mReturning) {
                ServerPlayer *shooter = nullptr;
                for (auto &playerEntry: mPlayers) {
                    if ((int64_t) playerEntry.second.getRuntimeId() == actor.getOwnerUniqueId() &&
                        playerEntry.second.isSpawned() && !playerEntry.second.isDead())
                        shooter = &playerEntry.second;
                }

                if (shooter == nullptr) {
                    dropProjectileItem(actor, actor.getPosition());
                    expired.push_back(actorId);
                    continue;
                }

                const Vector3f shooterPosition = shooter->getPosition();
                const Vector3f actorPosition = actor.getPosition();
                const float toX = shooterPosition.x - actorPosition.x;
                const float toY = shooterPosition.y + PLAYER_EYE_HEIGHT - actorPosition.y;
                const float toZ = shooterPosition.z - actorPosition.z;
                const float toLength = std::sqrt(toX * toX + toY * toY + toZ * toZ);

                if (toLength <= TRIDENT_RETURN_REACH) {
                    returnProjectileToOwner(*shooter, actor);
                    expired.push_back(actorId);
                    continue;
                }

                const float speed = TRIDENT_RETURN_SPEED * (float) actor.getProjectileData().mLoyaltyLevel;
                motion.x = toX / toLength * speed;
                motion.y = toY / toLength * speed;
                motion.z = toZ / toLength * speed;
            } else {
                motion.y -= thrown ? 0.03f : (arrowLike ? ARROW_GRAVITY : ACTOR_GRAVITY);
                if (thrown || arrowLike) {
                    motion.x *= 0.99f;
                    motion.y *= 0.99f;
                    motion.z *= 0.99f;
                }
            }

            const Vector3f previousPosition = actor.getPosition();

            Vector3f position = previousPosition;
            position.x += motion.x;
            position.y += motion.y;
            position.z += motion.z;

            actor.setMotion(motion);
            actor.setPosition(position);

            const int32_t blockX = (int32_t) std::floor(position.x);
            const int32_t blockY = (int32_t) std::floor(position.y);
            const int32_t blockZ = (int32_t) std::floor(position.z);

            if (arrowLike && actor.getProjectileData().mReturning) {
                actor.addLifetimeTick();
                broadcastActorMove(actor);
                continue;
            }

            if (actor.getLifetimeTicks() > 1 && level.isSolidAt(blockX, blockY, blockZ)) {
                const Vector3f hitPosition((float) blockX + 0.5f, (float) blockY + 0.5f, (float) blockZ + 0.5f);
                const Vector3i hitBlock(blockX, blockY, blockZ);
                const BlockState hitState = level.getBlockState(blockX, blockY, blockZ);
                const Block *block = VanillaBlocks::fromIdentifier(hitState.mName);
                if (block != nullptr && block->onProjectileHit(*this, level, hitBlock, hitState, actor)) {
                    expired.push_back(actorId);
                    continue;
                }

                if (!onThrownProjectileHit(actor, hitPosition, nullptr))
                    mScriptEngine.onProjectileHitBlock(actor, blockX, blockY, blockZ);

                if (!actor.getProjectileData().mReturning) {
                    expired.push_back(actorId);
                    continue;
                }

                actor.setPosition(previousPosition);
                actor.addLifetimeTick();
                broadcastActorMove(actor);
                continue;
            }

            if (actor.getLifetimeTicks() > 1) {
                const float sweepX = position.x - previousPosition.x;
                const float sweepY = position.y - previousPosition.y;
                const float sweepZ = position.z - previousPosition.z;
                const float sweepLength = std::sqrt(sweepX * sweepX + sweepY * sweepY + sweepZ * sweepZ);
                const int32_t sampleCount = std::max(1, (int32_t) std::ceil(sweepLength / 0.25f));

                ServerPlayer *hitPlayer = nullptr;
                ServerActor *hitActor = nullptr;
                Vector3f contactPosition = position;

                for (int32_t sample = 1; sample <= sampleCount && hitPlayer == nullptr && hitActor == nullptr;
                     ++sample) {
                    const float progress = (float) sample / (float) sampleCount;
                    const Vector3f samplePosition(previousPosition.x + sweepX * progress,
                                                  previousPosition.y + sweepY * progress,
                                                  previousPosition.z + sweepZ * progress);

                    for (auto &playerEntry: mPlayers) {
                        ServerPlayer &candidate = playerEntry.second;
                        if (!candidate.isSpawned() || candidate.getDimension() != actor.getDimension())
                            continue;
                        if ((int64_t) candidate.getRuntimeId() == actor.getOwnerUniqueId() &&
                            actor.getLifetimeTicks() < 8)
                            continue;

                        if (intersectsActorBox(candidate.getPosition(), PLAYER_WIDTH, PLAYER_HEIGHT,
                                               samplePosition)) {
                            hitPlayer = &candidate;
                            contactPosition = samplePosition;
                            break;
                        }
                    }

                    if (hitPlayer != nullptr)
                        break;

                    for (auto &actorEntry: mActors) {
                        ServerActor &candidate = *actorEntry.second;
                        if (&candidate == &actor || !candidate.isAlive() ||
                            candidate.getDimension() != actor.getDimension())
                            continue;

                        const bool candidateIsProjectile = candidate.isProjectile();
                        if (candidateIsProjectile && candidate.getOwnerUniqueId() == actor.getOwnerUniqueId() &&
                            candidate.getLifetimeTicks() < 8)
                            continue;

                        const ActorSize size = ActorSizeTable::getSize(candidate.getTypeId());
                        if (intersectsActorBox(candidate.getPosition(), size.mWidth, size.mHeight,
                                               samplePosition)) {
                            hitActor = &candidate;
                            contactPosition = samplePosition;
                            break;
                        }
                    }
                }

                if (hitPlayer != nullptr) {
                    onThrownProjectileHit(actor, contactPosition, hitPlayer);
                    expired.push_back(actorId);
                    continue;
                }

                if (hitActor != nullptr) {
                    onThrownProjectileHitActor(actor, contactPosition, *hitActor);
                    expired.push_back(actorId);
                    continue;
                }
            }

            MoveActorAbsolutePacket move;
            move.mRuntimeActorId = (int64_t) actor.getRuntimeId();
            move.mPosition = position;
            move.mRotation = actor.getRotation();

            for (auto &playerEntry: mPlayers) {
                if (playerEntry.second.isSpawned() && playerEntry.second.getDimension() == actor.getDimension())
                    mNetworkHandler->send(playerEntry.first, move, mCodecContext);
            }

            if (actor.getLifetimeTicks() > PROJECTILE_MAX_LIFETIME)
                expired.push_back(actorId);
            continue;
        }

        actor.tick(*this);

        if (actor.isExpired())
            expired.push_back(actorId);
    }

    std::vector<int64_t> expiredClouds;
    for (auto &entry: mLingeringClouds) {
        LingeringCloud &cloud = entry.second;
        cloud.mAge += 1;

        if (cloud.mAge > cloud.mWaitTime + cloud.mDuration) {
            expiredClouds.push_back(entry.first);
            continue;
        }

        if (cloud.mAge < cloud.mWaitTime)
            continue;

        cloud.mRadius += cloud.mRadiusPerTick;
        cloud.mNextApply -= 1;

        if (cloud.mNextApply <= 0) {
            cloud.mNextApply = cloud.mReapplicationDelay + LINGERING_CLOUD_APPLY_INTERVAL;

            bool touched = false;
            const float radiusSquared = cloud.mRadius * cloud.mRadius;

            for (auto &playerEntry: mPlayers) {
                ServerPlayer &nearby = playerEntry.second;
                if (!nearby.isSpawned() || nearby.getDimension() != cloud.mDimension)
                    continue;

                const Vector3f position = nearby.getPosition();
                const float dx = position.x - cloud.mPosition.x;
                const float dy = position.y - cloud.mPosition.y;
                const float dz = position.z - cloud.mPosition.z;
                if (dx * dx + dz * dz > radiusSquared || std::fabs(dy) > 1.0f)
                    continue;

                applyPotionEffects(nearby, cloud.mPotionId, 0.25f);
                touched = true;
            }

            for (auto &actorEntry: mActors) {
                ServerActor &nearby = *actorEntry.second;
                if (!nearby.isAlive() || nearby.isProjectile() || nearby.isDead() ||
                    nearby.getDimension() != cloud.mDimension)
                    continue;

                const Vector3f position = nearby.getPosition();
                const float dx = position.x - cloud.mPosition.x;
                const float dy = position.y - cloud.mPosition.y;
                const float dz = position.z - cloud.mPosition.z;
                if (dx * dx + dz * dz > radiusSquared || std::fabs(dy) > 1.0f)
                    continue;

                touched = true;
            }

            if (touched) {
                cloud.mRadius += cloud.mRadiusOnUse;
                cloud.mRadiusOnUse *= 0.5f;
            }
        }

        if (cloud.mRadius <= LINGERING_CLOUD_MIN_RADIUS) {
            expiredClouds.push_back(entry.first);
            continue;
        }

        if (cloud.mAge % 10 == 0) {
            ServerActor *cloudActor = getActor(entry.first);
            if (cloudActor != nullptr) {
                EntityDataMap metadata;

                EntityDataEntry radius;
                radius.mId = ACTOR_DATA_AREA_EFFECT_CLOUD_RADIUS;
                radius.mFormat = EntityDataFormat::Float;
                radius.mFloatValue = cloud.mRadius;
                metadata.mEntries.push_back(radius);

                EntityDataEntry width;
                width.mId = ACTOR_DATA_WIDTH;
                width.mFormat = EntityDataFormat::Float;
                width.mFloatValue = cloud.mRadius;
                metadata.mEntries.push_back(width);

                sendActorMetadata(*cloudActor, metadata);
            }
        }
    }

    for (const int64_t uniqueId: expiredClouds) {
        mLingeringClouds.erase(uniqueId);
        removeActor(uniqueId);
    }

    for (const int64_t uniqueId: expired)
        removeActor(uniqueId);
}
