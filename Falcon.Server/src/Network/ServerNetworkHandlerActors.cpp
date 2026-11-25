#include <cstring>
#include <chrono>
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Projectile/ProjectileReflection.h"
#include "World/Level/Explosion.h"
#include "World/Level/LevelSoundEventIds.h"
#include "World/Level/Block/Systems/DetectorSystem.h"

#include "World/Actor/Definition/KnockbackRules.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/DynamicPropertyStore.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Mob/Component/MobShooter.h"
#include "World/Actor/Movement/BandwidthOptimization.h"
#include "World/Actor/RideSystem.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Misc/PaintingActor.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "Server/Commands/ExecuteCommandOrigin.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "World/Actor/FreezingSystem.h"
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
#include "Protocol/Packets/UpdateAttributesPacket.h"
#include "Protocol/Packets/SetActorMotionPacket.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/SpawnParticleEffectPacket.h"
#include "Protocol/Packets/CameraInstructionPacket.h"
#include "Protocol/Packets/MobEffectPacket.h"
#include "Protocol/Packets/PlayerStartItemCooldownPacket.h"
#include "Protocol/Packets/SetPlayerGameTypePacket.h"
#include "Protocol/Packets/TakeItemActorPacket.h"
#include "Protocol/Packets/SetTitlePacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "World/Item/Item.h"
#include "World/Item/ItemData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/EnchantmentData.h"
#include "World/Level/Block/Systems/BlockContactSystem.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Actor/Misc/ExperienceOrbActor.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Actor/Projectile/ProjectileSystem.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Item/FireworkRocketItem.h"
#include "World/Item/PotionEffects.h"
#include "World/Item/StringToItemParser.h"
#include "World/Actor/Player/ExperienceValues.h"
#include "Protocol/Types/StartGameTypes.h"

#include <random>
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Scripting/Content/CustomContentRegistry.h"

#include <algorithm>
#include "Network/Handler/ChunkStreamHandler.h"
#include "World/Actor/Movement/MovementConstants.h"

#include <cmath>
#include <stdexcept>
#include <limits>
#include <utility>

namespace {
    const char *const UNDEAD_FAMILY = "undead";

    // a snow layer stops it inside its shape only; other blocks across the whole cell
    bool blocksProjectile(Level &level, const ServerActor &projectile, const Vector3f &position, int32_t x, int32_t y,
                          int32_t z) {
        if (!level.isSolidAt(x, y, z))
            return false;

        const BlockState *state = level.peekBlockPtr(x, y, z);
        const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
        if (block == nullptr || !block->shapesCollisionPerActor())
            return true;

        return BlockShape::isPositionInside(*state, x, y, z, position.x, position.y, position.z, &projectile);
    }

    MobEffectInstance potionEffectInstance(const PotionEffect &effect, float durationScale) {
        MobEffectInstance instance;
        instance.mId = effect.mId;
        instance.mDuration = std::max(1, (int32_t) ((float) effect.mDuration * durationScale));
        instance.mAmplifier = effect.mAmplifier;
        instance.mParticles = true;
        return instance;
    }

    bool allowProjectileHit(ServerNetworkHandler &owner, ServerActor &projectile, Level &level,
                            const Vector3f &position, Actor *target, const Vector3i *block) {
        EventSignal<ProjectileHitBeforeEvent> &hit = owner.getEventBus().before().mProjectileHit;
        if (!hit.hasSubscribers())
            return true;

        ProjectileHitBeforeEvent event(projectile, level, position, target,
                                       block != nullptr ? std::optional<Vector3i>(*block) : std::nullopt);
        hit.emit(event);
        return !event.isCancelled();
    }

    const int32_t ACTOR_DATA_SCALE = 38;
    const float ARROW_KNOCKBACK = 0.3f;
    const float PUNCH_KNOCKBACK_PER_LEVEL = 0.5f;
    const float IMPALING_DAMAGE_PER_LEVEL = 2.5f;
    const float TRIDENT_RETURN_SPEED = 0.6f;
    const float TRIDENT_RETURN_REACH = 1.5f;
    const float FIREWORK_HORIZONTAL_ACCELERATION = 1.15f;
    const float FIREWORK_VERTICAL_ACCELERATION = 0.04f;
    const int32_t FIREWORK_ITEM_DATA_ID = 16;
    const int32_t PROJECTILE_MAX_LIFETIME = 1200;
    const int32_t STUCK_ARROW_LIFETIME = 1200;
    // the player box grown by (1, 0.5, 1) picks up a planted arrow
    const float ARROW_PICKUP_REACH_XZ = 1.0f;
    const float ARROW_PICKUP_REACH_Y = 0.5f;
    // ActorEvent shake, with 7 as its data: the arrow quivers where it lands
    const uint8_t ACTOR_EVENT_SHAKE = 39;
    const int32_t ARROW_SHAKE_TIME = 7;
    const float ACTOR_SUFFOCATION_DAMAGE = 1.0f;
    const int32_t LINGERING_CLOUD_WAIT_TIME = 10;
    const int32_t LINGERING_CLOUD_DURATION = 600;
    const int32_t LINGERING_CLOUD_APPLY_INTERVAL = 10;
    const float LINGERING_CLOUD_RADIUS_PER_TICK = -0.005f;
    const float LINGERING_CLOUD_RADIUS_ON_USE = -0.5f;
    const float LINGERING_CLOUD_MIN_RADIUS = 1.5f;
    const float PROJECTILE_HIT_GROW = 0.3f;
    // blocks per axis a projectile sweep reads through the actor index; wider reads every actor once
    const float SWEEP_INDEX_SPAN = 64.0f;
    const int32_t EXPERIENCE_ORB_PICKUP_DELAY = 10;

    AxisAlignedBB projectileHitBox(const Vector3f &position, float width, float height) {
        const float half = width * 0.5f + PROJECTILE_HIT_GROW;
        return AxisAlignedBB(position.x - half, position.y - PROJECTILE_HIT_GROW, position.z - half,
                             position.x + half, position.y + height + PROJECTILE_HIT_GROW, position.z + half);
    }

    EntityProperties buildActorProperties(ServerActor &actor) {
        EntityProperties properties;

        const std::vector<ActorPropertyDescription> *schema = actor.getPropertySchema();
        if (schema == nullptr)
            return properties;

        for (const ActorPropertyDescription &descriptor: *schema) {
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

ServerActor *ServerNetworkHandler::spawnActor(Level &level, const std::string &identifier, const Vector3f &position,
                                              const std::function<void(ServerActor &)> &configure) {
    // taken first: configure may spawn another actor
    const bool spawned = mSpawnCause == nullptr;
    const char *cause = spawned ? "Spawned" : mSpawnCause;
    mSpawnCause = nullptr;
    const uint64_t runtimeId = allocateRuntimeId();

    // a summoned tnt is primed like one lit from a block
    std::unique_ptr<ServerActor> actor = identifier == PrimedTntActor::IDENTIFIER
                                                 ? std::unique_ptr<ServerActor>(new PrimedTntActor(
                                                           runtimeId, PrimedTntActor::DEFAULT_FUSE))
                                                 : ActorClassRegistry::create(runtimeId, identifier);
    actor->setUniqueId(allocateActorUniqueId());
    actor->getAttributes() = ActorAttributes::createActorDefaults();
    actor->moveToLevel(level);
    actor->setPosition(position);
    actor->resetFallDistance();

    MobActor *mob = dynamic_cast<MobActor *>(actor.get());
    if (mob != nullptr)
        mob->finalizeSpawn();

    // before configure: a projectile's callback reads its pack definition to aim it
    const CustomActorDefinition *definition = CustomContentRegistry::getInstance().getActorDefinition(identifier);
    if (definition != nullptr) {
        actor->setDefinition(definition);
        actor->setProjectile(definition->mIsProjectile);
    }

    if (configure)
        configure(*actor);

    // an identifier carrying its own spawn event makes the cause Event
    if (spawned && mob != nullptr && !mob->getSpawnEvent().empty())
        cause = "Event";

    if (mob != nullptr)
        mob->applyDefaults(mProperties.getDifficulty());

    if (mob != nullptr && mProperties.getDifficulty() == Difficulty::Peaceful && mob->shouldRemoveInPeaceful())
        return nullptr;

    actor->initializeProperties();

    EventSignal<ActorSpawnBeforeEvent> &spawn = mEventBus.before().mActorSpawn;
    if (spawn.hasSubscribers()) {
        ActorSpawnBeforeEvent event(level, *actor);
        spawn.emit(event);
        if (event.isCancelled())
            return nullptr;
    }

    ServerActor *result = _registerActor(std::move(actor));

    broadcastActorSpawn(*result);
    if (mob != nullptr)
        mob->upgradeDefinition(*this, false);
    mScriptEngine.onEntitySpawn(*result, cause);
    return result;
}

ServerActor *ServerNetworkHandler::spawnBabyActor(Level &level, const std::string &identifier, const Vector3f &position,
                                                  float scale) {
    mSpawnCause = "Born";
    ServerActor *baby = spawnActor(level, identifier, position);
    if (baby == nullptr)
        return nullptr;

    if (MobActor *mob = dynamic_cast<MobActor *>(baby))
        mob->markBorn();

    baby->getFlags().set(ActorFlag::Baby, true);

    EntityDataMap metadata;
    const int32_t flagIds[] = {ActorFlags::FLAGS_DATA_ID, ActorFlags::FLAGS_2_DATA_ID, ActorFlags::FLAGS_3_DATA_ID};
    const int64_t flagValues[] = {baby->getFlags().getLowBits(), baby->getFlags().getHighBits(),
                                  baby->getFlags().getExtendedBits()};
    for (int index = 0; index < 3; index++) {
        EntityDataEntry entry;
        entry.mId = flagIds[index];
        entry.mFormat = EntityDataFormat::Long;
        entry.mLongValue = flagValues[index];
        metadata.mEntries.push_back(entry);
    }

    EntityDataEntry scaleEntry;
    scaleEntry.mId = ACTOR_DATA_SCALE;
    scaleEntry.mFormat = EntityDataFormat::Float;
    scaleEntry.mFloatValue = scale;
    metadata.mEntries.push_back(scaleEntry);

    sendActorMetadata(*baby, metadata);
    return baby;
}

FallingBlock *ServerNetworkHandler::spawnFallingBlock(Level &level, const BlockState &state, const Vector3f &position) {
    const uint64_t runtimeId = allocateRuntimeId();

    std::unique_ptr<FallingBlock> actor(new FallingBlock(runtimeId, state));
    actor->getAttributes() = ActorAttributes::createActorDefaults();
    actor->moveToLevel(level);
    actor->setPosition(position);
    actor->setHighestPosition(position.y);

    FallingBlock *result = actor.get();
    _registerActor(std::move(actor));

    broadcastActorSpawn(*result);
    mScriptEngine.onEntitySpawn(*result);
    return result;
}

PrimedTntActor *ServerNetworkHandler::spawnPrimedTnt(Level &level, const Vector3f &position, const Vector3f &motion,
                                                     int32_t fuse) {
    const uint64_t runtimeId = allocateRuntimeId();

    std::unique_ptr<PrimedTntActor> actor(new PrimedTntActor(runtimeId, fuse));
    actor->getAttributes() = ActorAttributes::createActorDefaults();
    actor->moveToLevel(level);
    actor->setPosition(position);
    actor->setMotion(motion);

    PrimedTntActor *result = actor.get();
    _registerActor(std::move(actor));

    broadcastActorSpawn(*result);
    mScriptEngine.onEntitySpawn(*result);
    return result;
}

void ServerNetworkHandler::spawnExperienceOrbs(Level &level, const Vector3f &position, int amount) {
    if (amount <= 0)
        return;
    if (mDropCapture != nullptr) {
        mDropCapture->mExperience += amount;
        return;
    }

    static std::mt19937 orbRandom(0x1F123BB5u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    for (int value: ExperienceValues::splitIntoOrbSizes(amount)) {
        ServerActor *orb = spawnActor(level, "minecraft:xp_orb", position);
        if (orb == nullptr)
            continue;

        orb->setExperienceValue(value);
        orb->setPickupDelay(EXPERIENCE_ORB_PICKUP_DELAY);
        orb->setMotion(Vector3f((unit(orbRandom) * 0.2f - 0.1f) * 2.0f, unit(orbRandom) * 0.4f,
                                (unit(orbRandom) * 0.2f - 0.1f) * 2.0f));
    }
}

ServerActor *ServerNetworkHandler::getActor(int64_t uniqueId) {
    auto it = mActors.find(uniqueId);
    return it == mActors.end() ? nullptr : it->second.get();
}

ServerActor *ServerNetworkHandler::getActorByRuntimeId(uint64_t runtimeId) {
    auto it = mActorUniqueIdsByRuntimeId.find(runtimeId);
    return it == mActorUniqueIdsByRuntimeId.end() ? nullptr : getActor(it->second);
}

Actor *ServerNetworkHandler::getEntity(int64_t uniqueId) {
    if (ServerActor *actor = getActor(uniqueId))
        return actor;
    for (auto &entry: mPlayers) {
        if (entry.second.getUniqueId() == uniqueId)
            return &entry.second;
    }
    return nullptr;
}

int64_t ServerNetworkHandler::allocateActorUniqueId() {
    return (int64_t) (((uint64_t) getLevel().getWorldStartCount() << 32) | ++mActorUniqueIdCounter);
}

ServerActor *ServerNetworkHandler::_registerActor(std::unique_ptr<ServerActor> actor) {
    if (!actor->hasAssignedUniqueId() || mActors.count(actor->getUniqueId()) != 0)
        actor->setUniqueId(allocateActorUniqueId());

    ServerActor *result = actor.get();
    mActorUniqueIdsByRuntimeId[result->getRuntimeId()] = result->getUniqueId();
    mActors[result->getUniqueId()] = std::move(actor);
    mActorIndexDirty = true;
    ++mActorSetSerial;
    if (mJoinedTracked)
        mActorChanges.push_back(ActorChange{result, result->getPosition(), true});
    if (!result->getPendingPassengers().empty())
        mVehiclesWithPendingPassengers.insert(result->getUniqueId());
    return result;
}

void ServerNetworkHandler::_unregisterActor(int64_t uniqueId) {
    auto it = mActors.find(uniqueId);
    if (it == mActors.end())
        return;

    if (mJoinedTracked)
        mActorChanges.push_back(ActorChange{it->second.get(), it->second->getPosition(), false});
    mVehiclesWithPendingPassengers.erase(uniqueId);
    mActorUniqueIdsByRuntimeId.erase(it->second->getRuntimeId());
    // a walk may be running over the index: the slot goes null, erasing it would shift the walk's vector
    ServerActor *removed = it->second.get();
    std::replace(mCollidableActors.begin(), mCollidableActors.end(), removed, (ServerActor *) nullptr);
    if (mActorIndexWalks > 0) {
        for (auto &bucket: mActorIndex)
            std::replace(bucket.second.begin(), bucket.second.end(), removed, (ServerActor *) nullptr);
    }
    mActors.erase(it);
    mActorIndexDirty = true;
    ++mActorSetSerial;
}

void ServerNetworkHandler::_refreshActorIndex() {
    if (!mActorIndexDirty)
        return;

    mActorIndexDirty = false;
    // buckets cleared, not freed: no reallocation per tick; empty ones dropped
    for (auto it = mActorIndex.begin(); it != mActorIndex.end();) {
        if (it->second.empty()) {
            it = mActorIndex.erase(it);
            continue;
        }
        it->second.clear();
        ++it;
    }

    mActorIndexWidestHalf = 0.0f;
    mActorIndexWidestBox = 0.0f;
    mCollidableActors.clear();
    const std::string *lastType = nullptr;
    float lastHalf = 0.0f;
    for (auto &entry: mActors) {
        if (entry.second->getFlags().get(ActorFlag::Collidable))
            mCollidableActors.push_back(entry.second.get());
        const Vector3f position = entry.second->getPosition();
        const int32_t chunkX = (int32_t) std::floor(position.x) >> 4;
        const int32_t chunkZ = (int32_t) std::floor(position.z) >> 4;
        mActorIndex[_actorIndexKey(chunkX, chunkZ)].push_back(entry.second.get());
        // actors come in runs of one type: the size of the last type asked is kept
        const std::string &typeId = entry.second->getTypeId();
        if (lastType == nullptr || *lastType != typeId) {
            lastType = &typeId;
            lastHalf = ActorClassRegistry::getSize(typeId).mWidth * 0.5f;
        }
        mActorIndexWidestHalf = std::max(mActorIndexWidestHalf, lastHalf);
        mActorIndexWidestBox = std::max(mActorIndexWidestBox, entry.second->getSize().mWidth * 0.5f);
    }
}

void ServerNetworkHandler::_resolvePendingRides() {
    std::vector<int64_t> resolved;

    for (const int64_t vehicleId: mVehiclesWithPendingPassengers) {
        ServerActor *vehicle = getActor(vehicleId);
        if (vehicle == nullptr) {
            resolved.push_back(vehicleId);
            continue;
        }

        std::vector<int64_t> &pending = vehicle->getPendingPassengers();
        for (auto it = pending.begin(); it != pending.end();) {
            ServerActor *passenger = getActor(*it);
            if (passenger == nullptr) {
                ++it;
                continue;
            }

            RideSystem::mount(*this, *passenger, *vehicle, false);
            it = pending.erase(it);
        }

        if (pending.empty())
            resolved.push_back(vehicleId);
    }

    for (const int64_t vehicleId: resolved)
        mVehiclesWithPendingPassengers.erase(vehicleId);
}

// the client draws a projectile along its rotation, the reverse of the shooter's (yaw = -yaw, pitch = -pitch);
// head yaw follows yaw, or the arrow keeps pointing its spawn way
void ServerNetworkHandler::faceMotion(ServerActor &actor, const Vector3f &motion) {
    const float horizontal = std::sqrt(motion.x * motion.x + motion.z * motion.z);
    if (horizontal < 1.0e-6f && std::fabs(motion.y) < 1.0e-6f)
        return;
    const float yaw = std::atan2(motion.x, motion.z) / MathConstants::DEGREES_TO_RADIANS_F;
    const float pitch = std::atan2(motion.y, horizontal) / MathConstants::DEGREES_TO_RADIANS_F;
    actor.setRotation(Vector3f(pitch, yaw, yaw));
}

namespace {
    // the direction normalized, a gaussian of 0.0075 per uncertainty unit on each axis, times the power
    Vector3f shotMotion(const Vector3f &direction, float uncertainty, float power) {
        static std::mt19937 generator{std::random_device{}()};
        std::normal_distribution<float> gaussian(0.0f, 1.0f);
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y
                                       + direction.z * direction.z);
        Vector3f motion;
        if (length >= 0.001f)
            motion = Vector3f(direction.x / length, direction.y / length, direction.z / length);
        const float spread = 0.0075f * uncertainty;
        motion.x = (motion.x + gaussian(generator) * spread) * power;
        motion.y = (motion.y + gaussian(generator) * spread) * power;
        motion.z = (motion.z + gaussian(generator) * spread) * power;
        return motion;
    }
}

ServerActor *ServerNetworkHandler::spawnProjectile(ServerPlayer &player, const std::string &identifier,
                                                   float launchPower, float verticalOffset, const ItemStack *weapon) {
    Vector3f spawnPosition = player.getPosition();
    spawnPosition.y += player.getEyeHeight() + verticalOffset;

    float speed = 0.0f;
    // in the spawn packet: an arrow added at rotation 0 shows turning from there on the client
    ServerActor *projectile = spawnActor(getLevelFor(player), identifier, spawnPosition, [&](ServerActor &actor) {
        ProjectileComponent component;
        const bool defined = ProjectileComponent::findSpawned(*this, actor, player, component);
        if (defined)
            actor.getProjectileComponent() = component;
        else
            component = actor.getProjectileComponent();
        // no minecraft:projectile and no projectile class: nothing gives it a power, it stays put
        speed = defined || dynamic_cast<ProjectileActor *>(&actor) != nullptr ? component.mPower * launchPower : 0.0f;

        // the current game adds angle_offset to the pitch as is
        const Vector3f &rotation = player.getRotation();
        const float pitch = (rotation.x + component.mAngleOffset) * MathConstants::DEGREES_TO_RADIANS_F;
        const float yaw = rotation.y * MathConstants::DEGREES_TO_RADIANS_F;
        const Vector3f direction(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
        const float uncertainty = MobShooter::uncertainty(component.mUncertaintyBase, component.mUncertaintyMultiplier,
                                                          (int32_t) getProperties().getDifficulty());
        actor.setMotion(shotMotion(direction, uncertainty, speed));
        faceMotion(actor, actor.getMotion());
    });
    if (projectile == nullptr)
        return nullptr;

    projectile->setProjectile(true);
    projectile->setOwnerUniqueId(player.getUniqueId());
    projectile->getProjectileData().mLootingLevel = ItemEnchantments::getLevel(player.getInventory().getItemInHand(),
                                                                               EnchantmentIds::LOOTING);
    if (weapon != nullptr) {
        EventSignal<PlayerShootBowBeforeEvent> &shootBow = mEventBus.before().mPlayerShootBow;
        if (shootBow.hasSubscribers()) {
            const int64_t arrowId = projectile->getUniqueId();
            const uint64_t arrowRuntimeId = projectile->getRuntimeId();
            PlayerShootBowBeforeEvent event(player, *projectile, *weapon, speed);
            shootBow.emit(event);
            if (event.isCancelled()) {
                removeActor(arrowId);
                return nullptr;
            }
            projectile = getActorByRuntimeId(arrowRuntimeId);
            if (event.mReplacementRuntimeId) {
                const uint64_t replacementId = *event.mReplacementRuntimeId;
                auto *replacement = getActorByRuntimeId(replacementId);
                if (replacement != nullptr && replacement->sharesLevelWith(player)) {
                    if (replacementId != arrowRuntimeId)
                        removeActor(arrowId);
                    projectile = replacement;
                }
            }
            if (projectile == nullptr)
                return nullptr;
            projectile->setOwnerUniqueId(player.getUniqueId());
            const float force = event.mForce;
            const Vector3f current = projectile->getMotion();
            const float scale = speed > 0 ? force / speed : 0;
            projectile->setMotion(Vector3f(current.x * scale, current.y * scale, current.z * scale));
            faceMotion(*projectile, projectile->getMotion());
            broadcastActorMove(*projectile);
        }
    }
    return allowProjectileLaunch(*projectile, &player) ? projectile : nullptr;
}

// a projectile spawned without an owner, then shot along direction with uncertainty_base alone
ServerActor *ServerNetworkHandler::spawnDispensedProjectile(Level &level, const std::string &identifier,
                                                            const Vector3f &position, const Vector3f &direction,
                                                            const std::function<void(ServerActor &)> &configure) {
    // set before the spawn packet: a projectile added at rotation 0 shows turning from there on the client
    ServerActor *projectile = spawnActor(level, identifier, position, [&](ServerActor &actor) {
        if (configure)
            configure(actor);
        ProjectileComponent component;
        if (!ProjectileComponent::find(identifier, actor.getDefinition(), component))
            return;
        const Vector3f motion = shotMotion(direction, component.mUncertaintyBase, component.mPower);
        actor.setMotion(motion);
        faceMotion(actor, motion);
    });
    if (projectile == nullptr)
        return nullptr;

    projectile->setProjectile(true);
    return allowProjectileLaunch(*projectile, nullptr) ? projectile : nullptr;
}

bool ServerNetworkHandler::allowProjectileLaunch(ServerActor &projectile, Actor *shooter) {
    EventSignal<ProjectileLaunchBeforeEvent> &launch = mEventBus.before().mProjectileLaunch;
    if (!launch.hasSubscribers())
        return true;

    ProjectileLaunchBeforeEvent event(projectile, shooter);
    launch.emit(event);
    if (!event.isCancelled())
        return true;

    removeActor(projectile.getUniqueId());
    return false;
}

bool ServerNetworkHandler::onThrownProjectileHit(ServerActor &projectile, const Vector3f &hitPosition,
                                                 ServerPlayer *hitPlayer) {
    Level &level = getLevelFor(projectile);

    if (dynamic_cast<const ArrowActor *>(&projectile) != nullptr) {
        if (hitPlayer != nullptr)
            return onArrowProjectileHitTarget(projectile, hitPosition, *hitPlayer);

        ProjectileData &data = projectile.getProjectileData();
        const bool isTrident = dynamic_cast<const ThrownTridentActor *>(&projectile) != nullptr;
        // hit_ground_sound; arrow.json defines none and keeps
        // its bow.hit
        const int32_t groundSound = projectile.getProjectileComponent().mHitGroundSound;
        if (groundSound != LevelSoundEventIds::UNDEFINED)
            playLevelSound(level, LevelSoundEventIds::nameOf(groundSound), hitPosition);
        else if (!isTrident)
            playLevelSound(level, LevelSoundEvent::BOW_HIT, hitPosition);

        if (isTrident && data.mLoyaltyLevel > 0) {
            data.mReturning = true;
            playLevelSound(level, LevelSoundEvent::TRIDENT_RETURN, hitPosition);
            return false;
        }

        data.mStuck = true;
        return true;
    }

    ThrownProjectileActor *thrown = dynamic_cast<ThrownProjectileActor *>(&projectile);
    if (thrown != nullptr && thrown->onHit(*this, hitPosition, hitPlayer))
        return true;

    return thrown != nullptr;
}

void ServerNetworkHandler::spawnLingeringCloud(Level &level, const Vector3f &position, int32_t potionId) {
    AreaEffectCloudSettings settings;
    settings.mPotionId = potionId;
    settings.mRadius = 3.0f;
    settings.mRadiusPerTick = LINGERING_CLOUD_RADIUS_PER_TICK;
    settings.mRadiusOnUse = LINGERING_CLOUD_RADIUS_ON_USE;
    settings.mWaitTime = LINGERING_CLOUD_WAIT_TIME;
    settings.mDuration = LINGERING_CLOUD_DURATION;
    settings.mParticle = AREA_EFFECT_CLOUD_POTION_PARTICLE;
    spawnAreaEffectCloud(level, position, settings);
}

ServerActor *ServerNetworkHandler::spawnAreaEffectCloud(Level &level, const Vector3f &position,
                                                        const AreaEffectCloudSettings &settings) {
    ServerActor *cloud = spawnActor(level, "minecraft:area_effect_cloud", position);
    if (cloud == nullptr)
        return nullptr;

    EntityDataMap metadata;

    EntityDataEntry radius;
    radius.mId = ACTOR_DATA_AREA_EFFECT_CLOUD_RADIUS;
    radius.mFormat = EntityDataFormat::Float;
    radius.mFloatValue = settings.mRadius;
    metadata.mEntries.push_back(radius);

    EntityDataEntry particle;
    particle.mId = ACTOR_DATA_AREA_EFFECT_CLOUD_PARTICLE_ID;
    particle.mFormat = EntityDataFormat::Int;
    particle.mIntValue = settings.mParticle;
    metadata.mEntries.push_back(particle);

    EntityDataEntry auxValue;
    auxValue.mId = ACTOR_DATA_POTION_AUX_VALUE;
    auxValue.mFormat = EntityDataFormat::Short;
    auxValue.mShortValue = (int16_t) settings.mPotionId;
    metadata.mEntries.push_back(auxValue);

    EntityDataEntry color;
    color.mId = ACTOR_DATA_POTION_COLOR;
    color.mFormat = EntityDataFormat::Int;
    color.mIntValue = settings.mColor.has_value() ? *settings.mColor : getPotionColor(settings.mPotionId);
    metadata.mEntries.push_back(color);

    EntityDataEntry width;
    width.mId = ACTOR_DATA_WIDTH;
    width.mFormat = EntityDataFormat::Float;
    width.mFloatValue = settings.mRadius;
    metadata.mEntries.push_back(width);

    EntityDataEntry height;
    height.mId = ACTOR_DATA_HEIGHT;
    height.mFormat = EntityDataFormat::Float;
    height.mFloatValue = 0.5f;
    metadata.mEntries.push_back(height);

    sendActorMetadata(*cloud, metadata);

    LingeringCloud state;
    state.mPotionId = settings.mPotionId;
    state.mAge = 0;
    state.mWaitTime = settings.mWaitTime;
    state.mDuration = settings.mDuration;
    state.mNextApply = 0;
    state.mReapplicationDelay = settings.mReapplicationDelay;
    state.mRadius = settings.mRadius;
    state.mRadiusPerTick = settings.mRadiusPerTick;
    state.mRadiusOnUse = settings.mRadiusOnUse;
    state.mPosition = position;
    state.mDimension = cloud->getDimension();
    state.mWorldId = cloud->getWorldId();
    state.mOwnerUniqueId = settings.mOwnerUniqueId;
    mLingeringClouds[cloud->getUniqueId()] = state;
    return cloud;
}

void ServerNetworkHandler::broadcastLevelEvent(Level &level, int32_t eventId, const Vector3f &position, int32_t data) {
    LevelEventPacket event;
    event.mEventId = eventId;
    event.mPosition = position;
    event.mData = data;
    BlockActionHandler::broadcastToViewers(*this, level, position, event);
}

void ServerNetworkHandler::dropProjectileItem(ServerActor &projectile, const Vector3f &position) {
    const ProjectileData &data = projectile.getProjectileData();
    if (data.mPickupItem.isAir())
        return;

    ItemActorHandler::dropItem(*this, getLevelFor(projectile), position, data.mPickupItem,
                               ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
}

// stops at the last free point of this tick's path, so the arrow shows half sunk in the face it hit
void ServerNetworkHandler::plantProjectile(ServerActor &projectile, Level &level, const Vector3f &from,
                                           const Vector3i &block) {
    ProjectileData &data = projectile.getProjectileData();
    const Vector3f to = projectile.getPosition();
    Vector3f resting = from;
    const int32_t steps = 16;
    for (int32_t step = 1; step <= steps; ++step) {
        const float progress = (float) step / (float) steps;
        const Vector3f sample(from.x + (to.x - from.x) * progress, from.y + (to.y - from.y) * progress,
                              from.z + (to.z - from.z) * progress);
        if (blocksProjectile(level, projectile, sample, (int32_t) std::floor(sample.x), (int32_t) std::floor(sample.y),
                             (int32_t) std::floor(sample.z)))
            break;
        resting = sample;
    }

    data.mStuckBlock = block;
    data.mStuckTicks = 0;
    data.mCritical = false;
    projectile.setMotion(Vector3f());
    projectile.setPosition(resting);
    broadcastActorMove(projectile);

    ActorEventPacket shake;
    shake.mRuntimeActorId = projectile.getRuntimeId();
    shake.mEventId = ACTOR_EVENT_SHAKE;
    shake.mEventData = ARROW_SHAKE_TIME;
    shake.mHasFirePosition = false;
    _sendToViewers(projectile, shake);
}

// true once the planted projectile is gone: picked up or expired
bool ServerNetworkHandler::tickPlantedProjectile(ServerActor &projectile, Level &level) {
    ProjectileData &data = projectile.getProjectileData();
    if (++data.mStuckTicks >= STUCK_ARROW_LIFETIME)
        return true;

    const Vector3i &block = data.mStuckBlock;
    if (!level.isSolidAt(block.x, block.y, block.z)) {
        // the block went: the arrow falls again
        data.mStuck = false;
        return false;
    }

    const Vector3f &position = projectile.getPosition();
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.sharesLevelWith(projectile)
            || player.isSpectator())
            continue;

        const Vector3f &feet = player.getPosition();
        const float halfWidth = player.getBodyWidth() * 0.5f + ARROW_PICKUP_REACH_XZ;
        if (std::fabs(position.x - feet.x) > halfWidth || std::fabs(position.z - feet.z) > halfWidth
            || position.y < feet.y - ARROW_PICKUP_REACH_Y
            || position.y > feet.y + player.getBodyHeight() + ARROW_PICKUP_REACH_Y)
            continue;

        // nothing to give outside creative: the arrow stays until it expires
        const bool creative = player.isCreative();
        if (!creative && (data.mPickupCreativeOnly || data.mPickupItem.isAir()))
            continue;

        if (!creative) {
            std::vector<int> touched;
            if (!player.getInventory().addItem(data.mPickupItem, touched))
                continue;
            for (int touchedSlot: touched)
                player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, touchedSlot);
        }

        TakeItemActorPacket take;
        take.mItemRuntimeActorId = projectile.getRuntimeId();
        take.mRuntimeActorId = player.getRuntimeId();
        _sendToViewers(projectile, take);
        return true;
    }

    return false;
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

    int32_t damage = data.mImpactDamage >= 0 ? data.mImpactDamage : (int32_t) std::ceil(speed * data.mBaseDamage);
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
    const bool isTrident = dynamic_cast<const ThrownTridentActor *>(&projectile) != nullptr;

    float damage = computeProjectileDamage(projectile);
    if (isTrident && data.mImpalingLevel > 0 && LiquidBlocksFetch::at(getLevelFor(target), target.getPosition()).water)
        damage += IMPALING_DAMAGE_PER_LEVEL * (float) data.mImpalingLevel;

    ServerPlayer *shooter = nullptr;
    for (auto &entry: mPlayers) {
        if (entry.second.getUniqueId() == projectile.getOwnerUniqueId())
            shooter = &entry.second;
    }

    bool applyArrowEffect = false;
    ServerPlayer *victimPlayer = dynamic_cast<ServerPlayer *>(&target);
    if (victimPlayer != nullptr) {
        const Vector3f motion = projectile.getMotion();
        const Vector3f origin(hitPosition.x - motion.x, hitPosition.y - motion.y, hitPosition.z - motion.z);

        ActorDamageSource source = ActorDamageSource::environment("death.attack.arrow", victimPlayer->getName());
        source.mDeathMessageParameters.push_back(shooter == nullptr ? std::string() : shooter->getName());
        source.mAttacker = shooter;
        source.fromOrigin(origin).asProjectile().withDamager(&projectile);

        const DamageResult result = hurt(*victimPlayer, damage, source);
        applyArrowEffect = result == DamageResult::Dealt;
        if (result == DamageResult::Blocked) {
            if (!isTrident)
                return true;

            data.mHadCollision = true;
            if (data.mLoyaltyLevel > 0 && shooter != nullptr) {
                data.mReturning = true;
                playLevelSound(getLevelFor(projectile), LevelSoundEvent::TRIDENT_RETURN, hitPosition);
                return false;
            }

            dropProjectileItem(projectile, hitPosition);
            return true;
        }
    } else {
        ServerActor *victimActor = dynamic_cast<ServerActor *>(&target);
        if (victimActor != nullptr) {
            ActorDamageSource source = ActorDamageSource::environment("death.attack.arrow", victimActor->getName());
            source.mAttacker = shooter != nullptr ? (Actor *) shooter
                                                  : (Actor *) getActor(projectile.getOwnerUniqueId());
            applyArrowEffect = damageActor(*victimActor, damage, source.asProjectile().withDamager(&projectile),
                                           data.mLootingLevel);
            if (!isTrident && victimActor->stopsProjectiles())
                return true;
        }
    }

    if (!isTrident && applyArrowEffect && data.mPotionId >= 0 && target.isAlive()) {
        if (victimPlayer != nullptr)
            applyPotionEffects(*victimPlayer, data.mPotionId, 0.125f);
        else if (auto *victim = dynamic_cast<ServerActor *>(&target))
            applyPotionEffects(*victim, data.mPotionId, 0.125f);
    }

    const Vector3f targetPosition = target.getPosition();
    const float knockback = ARROW_KNOCKBACK + PUNCH_KNOCKBACK_PER_LEVEL * (float) data.mPunchLevel;
    knockBack(target, targetPosition.x - hitPosition.x, targetPosition.z - hitPosition.z, knockback);

    const int32_t flameTicks = data.mFlameTicks > 0 && !target.hasEffect(MobEffectId::FireResistance)
                              ? allowCombustion(target, data.mFlameTicks, &projectile) : 0;
    if (flameTicks > 0) {
        target.setFireTicks(flameTicks);
        target.setOnFire(true);

        ServerActor *burningActor = dynamic_cast<ServerActor *>(&target);
        if (burningActor != nullptr)
            syncActorFlags(*burningActor);
        else if (victimPlayer != nullptr)
            _sendEntityData(*victimPlayer);
    }

    Level &level = getLevelFor(projectile);
    // hit_sound: bow.hit and item.trident.hit in the vanilla definitions
    const int32_t hitSound = projectile.getProjectileComponent().mHitSound;
    if (hitSound != LevelSoundEventIds::UNDEFINED)
        playLevelSound(level, LevelSoundEventIds::nameOf(hitSound), hitPosition);

    if (isTrident) {
        data.mHadCollision = true;

        if (projectile.getProjectileComponent().mLightning && level.hasSkyLight()
            && getWorldOf(level).getOverworld().isThundering())
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
    if (dynamic_cast<const ArrowActor *>(&projectile) != nullptr)
        return onArrowProjectileHitTarget(projectile, hitPosition, hitActor);

    if (hitActor.isProjectile()) {
        const bool handled = onThrownProjectileHit(projectile, hitPosition, nullptr);
        const bool otherHandled = onThrownProjectileHit(hitActor, hitActor.getPosition(), nullptr);
        // an arrow knocked out of the air is removed, not planted: its item drops
        if (hitActor.getProjectileData().mStuck)
            dropProjectileItem(hitActor, hitActor.getPosition());
        removeActor(hitActor.getUniqueId());
        return handled || otherHandled;
    }

    ThrownProjectileActor *thrown = dynamic_cast<ThrownProjectileActor *>(&projectile);
    if (thrown != nullptr && thrown->onHitActor(*this, hitPosition, hitActor))
        return true;

    return onThrownProjectileHit(projectile, hitPosition, nullptr);
}

void ServerNetworkHandler::applyPotionEffects(ServerPlayer &player, int32_t potionId, float durationScale) {
    for (const PotionEffect &effect: getPotionEffects(potionId)) {
        if (effect.mInstant) {
            if (effect.mId == MobEffectId::InstantHealth) {
                player.heal(4.0f * (float) (1 << effect.mAmplifier));
            } else if (effect.mId == MobEffectId::InstantDamage) {
                hurt(player, 6.0f * (float) (1 << effect.mAmplifier),
                     ActorDamageSource::environment("death.attack.magic", player.getName())
                             .withoutArmor()
                             .withoutCooldown());
            }
            continue;
        }

        player.addEffect(potionEffectInstance(effect, durationScale));
    }
}

void ServerNetworkHandler::applyPotionEffects(ServerActor &actor, int32_t potionId, float durationScale) {
    const MobActor *mob = dynamic_cast<const MobActor *>(&actor);
    const std::vector<std::string> families = mob == nullptr ? std::vector<std::string>() : mob->getFamilies();
    const bool undead = std::find(families.begin(), families.end(), UNDEAD_FAMILY) != families.end();

    for (const PotionEffect &effect: getPotionEffects(potionId)) {
        if (!effect.mInstant) {
            actor.addEffect(potionEffectInstance(effect, durationScale));
            continue;
        }

        if (effect.mId != MobEffectId::InstantHealth && effect.mId != MobEffectId::InstantDamage)
            continue;

        if (!actor.canBeAffected(effect.mId))
            continue;

        if ((effect.mId == MobEffectId::InstantHealth) != undead) {
            actor.heal(4.0f * (float) (1 << effect.mAmplifier));
            syncActorAttributes(actor);
        } else {
            damageActor(actor, 6.0f * (float) (1 << effect.mAmplifier),
                        ActorDamageSource::environment("death.attack.magic", actor.getName()).withoutArmor());
        }
    }
}

void ServerNetworkHandler::_applyCloudEffects(ServerPlayer &player, const std::vector<MobEffectInstance> &effects) {
    for (const MobEffectInstance &effect: effects) {
        if (effect.mId == MobEffectId::InstantHealth) {
            player.heal(std::ldexp(4.0f, std::min(effect.mAmplifier, 30)));
        } else if (effect.mId == MobEffectId::InstantDamage) {
            hurt(player, std::ldexp(6.0f, std::min(effect.mAmplifier, 30)),
                 ActorDamageSource::environment("death.attack.magic", player.getName()).withoutArmor().withoutCooldown());
        } else {
            player.addEffect(effect);
        }
    }
}

void ServerNetworkHandler::_applyCloudEffects(ServerActor &actor, const std::vector<MobEffectInstance> &effects) {
    const auto *mob = dynamic_cast<const MobActor *>(&actor);
    const auto families = mob != nullptr ? mob->getFamilies() : std::vector<std::string>();
    const bool undead = std::find(families.begin(), families.end(), UNDEAD_FAMILY) != families.end();
    for (const MobEffectInstance &effect: effects) {
        if (effect.mId != MobEffectId::InstantHealth && effect.mId != MobEffectId::InstantDamage) {
            actor.addEffect(effect);
        } else if (actor.canBeAffected(effect.mId)) {
            if ((effect.mId == MobEffectId::InstantHealth) != undead) {
                actor.heal(std::ldexp(4.0f, std::min(effect.mAmplifier, 30)));
                syncActorAttributes(actor);
            } else {
                damageActor(actor, std::ldexp(6.0f, std::min(effect.mAmplifier, 30)),
                            ActorDamageSource::environment("death.attack.magic", actor.getName()).withoutArmor());
            }
        }
    }
}

void ServerNetworkHandler::removeActor(int64_t uniqueId) {
    auto it = mActors.find(uniqueId);
    if (it == mActors.end())
        return;

    mScriptEngine.onEntityRemove(*it->second);
    it = mActors.find(uniqueId);
    if (it == mActors.end())
        return;

    RideSystem::ejectAll(*this, *it->second);
    if (it->second->isRiding())
        RideSystem::dismount(*this, *it->second, false);

    broadcastActorRemove(*it->second);
    getLevelFor(*it->second).eraseEntity(uniqueId);
    _unregisterActor(uniqueId);
}

bool ServerNetworkHandler::canPlayerSeeActor(ServerPlayer &player, const Actor &actor) {
    if (!player.isSpawned() || !player.sharesLevelWith(actor))
        return false;

    const Vector3f position = actor.getPosition();
    if (!ChunkStreamHandler::isInActorRange(player.getPosition(), position))
        return false;

    const int32_t chunkX = (int32_t) std::floor(position.x) >> 4;
    const int32_t chunkZ = (int32_t) std::floor(position.z) >> 4;
    // an actor goes out with its chunk: one in a chunk this client has not received floats in the void
    if (player.getSentChunks().count(ChunkStreamHandler::packChunk(chunkX, chunkZ)) == 0)
        return false;

    // only chunks the server holds fully loaded hand out their actors
    if (!getLevelFor(actor).isChunkPopulated(chunkX, chunkZ))
        return false;

    // a plugin draws it itself: never in a visible set, so no spawn, move or data of Falcon's goes out for it
    const auto *drawn = dynamic_cast<const ServerActor *>(&actor);
    return drawn == nullptr || !drawn->isDrawnByPlugin();
}

void ServerNetworkHandler::_sendActorSpawn(ServerPlayer &player, ServerActor &actor) {
    if (const auto *painting = dynamic_cast<const PaintingActor *>(&actor)) {
        painting->sendPaintingSpawn(*this, player);
        return;
    }

    AddActorPacket packet;
    packet.mUniqueActorId = actor.getUniqueId();
    packet.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    packet.mIdentifier = actor.getTypeId();
    packet.mPosition = actor.getPosition();
    packet.mPosition.y += actor.getBaseOffset();
    packet.mMotion = actor.getMotion();
    packet.mRotation = Vector2f(actor.getRotation().x, actor.getRotation().y);
    packet.mHeadRotation = actor.getRotation().z;
    packet.mBodyRotation = actor.getRotation().y;
    packet.mProperties = buildActorProperties(actor);
    packet.mAttributes = actor.getAttributes().getAll();
    RideSystem::appendLinks(actor, packet.mActorLinks);
    actor.fillSpawnMetadata(packet.mMetadata);
    if (actor.getLimitedLifeTicks() > 0) {
        const int32_t ENTITY_DATA_LIFETIME_TICKS = 78;
        EntityDataEntry lifetime;
        lifetime.mId = ENTITY_DATA_LIFETIME_TICKS;
        lifetime.mFormat = EntityDataFormat::Int;
        lifetime.mIntValue = actor.getLimitedLifeTicks();
        packet.mMetadata.mEntries.push_back(lifetime);
    }

    if (!actor.getNameTag().empty()) {
        const int32_t ENTITY_DATA_NAME = 4;
        EntityDataEntry name;
        name.mId = ENTITY_DATA_NAME;
        name.mFormat = EntityDataFormat::String;
        name.mStringValue = actor.getNameTag();
        packet.mMetadata.mEntries.push_back(name);
    }

    const int64_t visibleEffects = actor.getVisibleEffectsData();
    if (visibleEffects != 0) {
        EntityDataEntry effects;
        effects.mId = ActorFlags::VISIBLE_MOB_EFFECTS_DATA_ID;
        effects.mFormat = EntityDataFormat::Long;
        effects.mLongValue = visibleEffects;
        packet.mMetadata.mEntries.push_back(effects);
    }

    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);

    if (MobActor *mob = dynamic_cast<MobActor *>(&actor))
        mob->getEquipment().sendTo(*this, player, actor);
    else
        actor.sendEquipmentTo(*this, player);
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

void ServerNetworkHandler::refreshEntityContainerViewers(int64_t actorId, const ServerPlayer *except) {
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (&player != except && player.isSpawned())
            player.getInventoryManager().refreshEntityContainer(actorId);
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
    // every offset the range table holds fits in this many chunks
    static const int32_t RANGE_CHUNKS = 4;
    static std::vector<uint64_t> seenIds;

    mActorIndexDirty = true;
    _refreshActorIndex();
    ItemActorHandler::indexForViewers(*this);
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned())
            continue;

        std::unordered_set<uint64_t> &visible = player.getVisibleActors();

        // the ones seen, then the ones near: never every actor for every player
        seenIds.assign(visible.begin(), visible.end());
        for (const uint64_t runtimeId: seenIds) {
            ServerActor *actor = getActorByRuntimeId(runtimeId);
            if (actor == nullptr) {
                // dropped items share the set and its ids, not the actor table
                ItemActorHandler::hideIfOutOfView(*this, player, runtimeId);
                continue;
            }

            // an autonomous actor stays known once sent, wherever it goes
            const bool kept = actor->isAutonomous() && player.sharesLevelWith(*actor);
            if ((actor->isAlive() || actor->isDead()) && (kept || canPlayerSeeActor(player, *actor)))
                continue;

            visible.erase(runtimeId);
            _sendActorRemove(player, *actor);
        }

        const Vector3f position = player.getPosition();
        const int32_t centreX = (int32_t) std::floor(position.x) >> 4;
        const int32_t centreZ = (int32_t) std::floor(position.z) >> 4;
        for (int32_t chunkX = centreX - RANGE_CHUNKS; chunkX <= centreX + RANGE_CHUNKS; chunkX++) {
            for (int32_t chunkZ = centreZ - RANGE_CHUNKS; chunkZ <= centreZ + RANGE_CHUNKS; chunkZ++) {
                ItemActorHandler::showColumnTo(*this, player, chunkX, chunkZ);

                const auto bucket = mActorIndex.find(_actorIndexKey(chunkX, chunkZ));
                if (bucket == mActorIndex.end())
                    continue;

                for (ServerActor *actor: bucket->second) {
                    if (actor == nullptr || !actor->isAlive() || visible.find(actor->getRuntimeId()) != visible.end()
                        || !canPlayerSeeActor(player, *actor))
                        continue;

                    visible.insert(actor->getRuntimeId());
                    _sendActorSpawn(player, *actor);
                }
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

void ServerNetworkHandler::changeActorDimension(Actor &actor, DimensionType dimension, const Vector3f &position) {
    changeActorLevel(actor, getWorldFor(actor).getLevel(dimension), position);
}

void ServerNetworkHandler::changeActorLevel(Actor &actor, Level &destination, const Vector3f &position) {
    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&actor)) {
        changePlayerLevel(*player, destination, position);
        return;
    }

    ServerActor *traveller = dynamic_cast<ServerActor *>(&actor);
    if (traveller == nullptr)
        return;

    const MobActor *mob = dynamic_cast<const MobActor *>(traveller);
    if (mob != nullptr && mob->getComponent("minecraft:dimension_bound") != nullptr)
        return;

    broadcastActorRemove(*traveller);
    traveller->moveToLevel(destination);
    traveller->setPosition(position);
    traveller->setMotion(Vector3f(0.0f, 0.0f, 0.0f));
    traveller->resetFallDistance();
    detachActorIfUnloaded(*traveller, destination);
}

bool ServerNetworkHandler::detachActorIfUnloaded(ServerActor &actor, Level &level) {
    const Vector3f &position = actor.getPosition();
    const int32_t chunkX = (int32_t) std::floor(position.x) >> 4;
    const int32_t chunkZ = (int32_t) std::floor(position.z) >> 4;
    const int64_t column = ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    if (getWorldOf(level).getActorLoadedChunks(level.getDimensionType()).count(column) != 0)
        return false;

    if (actor.shouldSave() && level.isStorageOpen()) {
        std::vector<Tag> entities = level.loadEntities(chunkX, chunkZ);
        entities.push_back(actor.saveNbt());
        level.saveEntities(chunkX, chunkZ, entities);
    }

    mDetachedActors.push_back(actor.getUniqueId());
    level.releaseChunkIfUnused(chunkX, chunkZ);
    return true;
}

void ServerNetworkHandler::syncActorAttributes(ServerActor &actor) {
    UpdateAttributesPacket packet;
    packet.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    packet.mTick = 0;
    packet.mAttributes = actor.getAttributes().getAll();

    _sendToViewers(actor, packet);
}

void ServerNetworkHandler::syncActorScale(ServerActor &actor, float scale) {
    EntityDataMap metadata;
    EntityDataEntry entry;
    entry.mId = ACTOR_DATA_SCALE;
    entry.mFormat = EntityDataFormat::Float;
    entry.mFloatValue = scale;
    metadata.mEntries.push_back(entry);
    sendActorMetadata(actor, metadata);
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


    EntityDataEntry flags3;

    flags3.mId = ActorFlags::FLAGS_3_DATA_ID;

    flags3.mFormat = EntityDataFormat::Long;

    flags3.mLongValue = actor.getFlags().getExtendedBits();
    packet.mMetadata.mEntries.push_back(flags3);


    EntityDataEntry nameplateDistance;

    nameplateDistance.mId = ActorFlags::NAMEPLATE_RENDER_DISTANCE_DATA_ID;

    nameplateDistance.mFormat = EntityDataFormat::Float;

    nameplateDistance.mFloatValue = actor.getNameplateRenderDistance();

    packet.mMetadata.mEntries.push_back(nameplateDistance);

    EntityDataEntry visibleEffects;
    visibleEffects.mId = ActorFlags::VISIBLE_MOB_EFFECTS_DATA_ID;
    visibleEffects.mFormat = EntityDataFormat::Long;
    visibleEffects.mLongValue = actor.getVisibleEffectsData();
    packet.mMetadata.mEntries.push_back(visibleEffects);

    packet.mProperties = buildActorProperties(actor);

    _sendToViewers(actor, packet);
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

void ServerNetworkHandler::queueActorCommand(ServerActor &actor, const std::string &command) {
    QueuedActorCommand queued;
    queued.mLevel = &getLevelFor(actor);
    queued.mPosition = actor.getPosition();
    queued.mRotation = actor.getRotation();
    queued.mCommand = !command.empty() && command[0] == '/' ? command.substr(1) : command;
    mQueuedActorCommands.push_back(std::move(queued));
}

void ServerNetworkHandler::runQueuedActorCommands() {
    if (mQueuedActorCommands.empty())
        return;

    std::vector<QueuedActorCommand> queued;
    queued.swap(mQueuedActorCommands);

    // pack content: game director level, never the console's
    ServerCommandOrigin base(this, CommandPermission::GameDirectors);
    for (const QueuedActorCommand &entry: queued) {
        ExecuteCommandOrigin origin(base, nullptr, entry.mPosition, entry.mRotation, entry.mLevel);
        mCommands.dispatch(origin, entry.mCommand);
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

void ServerNetworkHandler::knockBack(Actor &actor, float deltaX, float deltaZ, const KnockbackPreset &preset) {
    actor.knockBack(deltaX, deltaZ, preset.mHorizontalPower, preset.mVerticalPower, preset.mVerticalVelocityCap,
                    preset.mSlowdownScale);
    sendActorMotion(actor);
}

void ServerNetworkHandler::pushFrom(Actor &actor, const Vector3f &origin, float strength, float lift) {
    const Vector3f position = actor.getPosition();

    Vector3f motion = actor.getMotion();
    motion.x = motion.x * 0.5f - (origin.x - position.x) * strength;
    motion.y = motion.y * 0.5f + lift;
    motion.z = motion.z * 0.5f - (origin.z - position.z) * strength;

    actor.setMotion(motion);
    if (actor.isPlayer())
        static_cast<ServerPlayer &>(actor).scheduleKnockback(motion);
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

bool ServerNetworkHandler::damageActor(ServerActor &actor, float amount, Actor *attacker, int32_t lootingLevel) {
    return actor.hurt(*this, amount, attacker, lootingLevel);
}

bool ServerNetworkHandler::damageActor(ServerActor &actor, float amount, const ActorDamageSource &source,
                                       int32_t lootingLevel) {
    return actor.hurt(*this, amount, source, lootingLevel);
}

int32_t ServerNetworkHandler::allowCombustion(Actor &victim, int32_t duration, Actor *source,
                                               std::optional<Vector3i> block) {
    EventSignal<ActorCombustBeforeEvent> &combust = block ? mEventBus.before().mActorCombustByBlock
                                                          : mEventBus.before().mActorCombustByActor;
    if (!combust.hasSubscribers())
        return duration;

    ActorCombustBeforeEvent event(victim, source, block, duration);
    combust.emit(event);
    return event.isCancelled() ? 0 : event.mDuration;
}

void ServerNetworkHandler::hurtActor(Actor &actor, float amount, const std::string &deathMessageKey,
                                      std::optional<Vector3i> block) {
    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&actor)) {
        hurt(*player, amount, ActorDamageSource::environment(deathMessageKey, player->getName()).fromBlock(block));
        return;
    }

    if (ServerActor *target = dynamic_cast<ServerActor *>(&actor))
        target->hurt(*this, amount, ActorDamageSource::environment(deathMessageKey, target->getName()).fromBlock(block));
}

bool ServerNetworkHandler::broadcastActorMove(ServerActor &actor, bool teleported, const ServerPlayer *except) {
    MoveActorAbsolutePacket move;
    move.mRuntimeActorId = (int64_t) actor.getRuntimeId();
    move.mPosition = actor.getPosition();
    move.mPosition.y += actor.getBaseOffset();
    move.mRotation = actor.getRotation();
    move.mTeleported = teleported;

    BandwidthOptimization::Values values;
    if (teleported || !BandwidthOptimization::valuesOf(*this, actor, values)) {
        _sendToViewers(actor, move, except);
        return true;
    }

    const int64_t tick = getCurrentTick();
    const bool onGround = actor.isOnGround();
    const Vector3f position = actor.getPosition();
    std::unordered_map<uint64_t, ServerActor::MoveSent> &sent = actor.getMovesSent();
    bool everyone = true;
    _sendToViewers(actor, move, except, [&](ServerPlayer &player) {
        ServerActor::MoveSent &last = sent[player.getRuntimeId()];
        int64_t gap = 0;
        // a landing or a lift-off goes out at once
        if (last.mOnGround == onGround) {
            const Vector3f &viewer = player.getPosition();
            const float dx = position.x - viewer.x;
            const float dy = position.y - viewer.y;
            const float dz = position.z - viewer.z;
            gap = BandwidthOptimization::allowedGap(values, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        if (tick < last.mTick + gap) {
            everyone = false;
            return false;
        }
        last.mTick = tick;
        last.mOnGround = onGround;
        return true;
    });
    return everyone;
}

void ServerNetworkHandler::_sendToViewers(const ServerActor &actor, const Packet &packet, const ServerPlayer *except,
                                          const std::function<bool(ServerPlayer &)> &accept) {
    // kept between calls: a crowd sends one packet an actor a tick
    static std::vector<NetworkIdentifier> viewers;
    viewers.clear();
    const auto consider = [&](ServerPlayer &player) {
        if (!player.isSpawned() || &player == except)
            return;

        const std::unordered_set<uint64_t> &visible = player.getVisibleActors();
        if (visible.find(actor.getRuntimeId()) != visible.end() && (!accept || accept(player)))
            viewers.push_back(player.getNetworkIdentifier());
    };
    if (actor.isAutonomous()) {
        // known wherever it goes: its viewers can be anywhere
        for (auto &entry: mPlayers)
            consider(entry.second);
    } else {
        // a viewer was in range at the last visibility pass: one chunk more covers the move since
        static const int32_t VIEWER_CHUNKS = 6;
        const Vector3f position = actor.getPosition();
        forEachPlayerAroundChunk((int32_t) std::floor(position.x) >> 4, (int32_t) std::floor(position.z) >> 4,
                                 VIEWER_CHUNKS, consider);
    }
    mNetworkHandler->sendToMany(viewers, packet, mCodecContext);
}

void ServerNetworkHandler::playActorAnimation(ServerActor &actor, const std::string &animation) {
    AnimateEntityPacket packet;
    packet.mAnimation = animation;
    packet.mRuntimeActorIds.push_back(actor.getRuntimeId());
    broadcastToSpawned(packet);
}

void ServerNetworkHandler::broadcastToSpawned(const Packet &packet) {
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::spawnParticleEffect(Level &level, const std::string &identifier, const Vector3f &position) {
    spawnParticleEffect(level, identifier, position, nullptr, nullptr);
}

void ServerNetworkHandler::spawnParticleEffect(Level &level, const std::string &identifier, const Vector3f &position,
                                               const std::string *molangJson, ServerPlayer *viewer) {
    SpawnParticleEffectPacket packet;
    packet.mDimensionId = level.getDimensionId();
    packet.mUniqueActorId = -1;
    packet.mPosition = position;
    packet.mIdentifier = identifier;
    packet.mHasMolangVariablesJson = molangJson != nullptr;
    if (molangJson != nullptr)
        packet.mMolangVariablesJson = *molangJson;

    EventSignal<WorldEffectBeforeEvent> &particle = mEventBus.before().mWorldParticle;
    if (particle.hasSubscribers()) {
        std::vector<NetworkIdentifier> viewers;
        for (auto &entry: mPlayers) {
            if (entry.second.isSpawned() && entry.second.isIn(level) && (viewer == nullptr || &entry.second == viewer))
                viewers.push_back(entry.first);
        }
        WorldEffectBeforeEvent event(level, position, packet, viewers);
        particle.emit(event);
        if (!event.isCancelled())
            mNetworkHandler->sendToMany(viewers, packet, mCodecContext);
        return;
    }

    if (viewer != nullptr) {
        if (viewer->isSpawned())
            mNetworkHandler->send(viewer->getNetworkIdentifier(), packet, mCodecContext);
        return;
    }
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned() && entry.second.isIn(level))
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

void ServerNetworkHandler::playBlockSound(Level &level, const std::string &sound, const Vector3f &position,
                                          int32_t blockHash, const std::string &actorType) {
    LevelSoundEventPacket packet;
    packet.mSound = sound;
    packet.mPosition = position;
    packet.mExtraData = blockHash;
    packet.mExtraDataIsBlock = true;
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

    EventSignal<WorldEffectBeforeEvent> &worldSound = mEventBus.before().mWorldSound;
    if (worldSound.hasSubscribers()) {
        std::vector<NetworkIdentifier> viewers;
        for (auto &entry: mPlayers) {
            if (entry.second.isSpawned() && entry.second.isIn(level))
                viewers.push_back(entry.first);
        }
        WorldEffectBeforeEvent event(level, position, packet, viewers);
        worldSound.emit(event);
        if (!event.isCancelled())
            mNetworkHandler->sendToMany(viewers, packet, mCodecContext);
        return;
    }

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned() && entry.second.isIn(level))
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
    EventSignal<WorldEffectBeforeEvent> &worldSound = mEventBus.before().mWorldSound;
    if (worldSound.hasSubscribers()) {
        std::vector<NetworkIdentifier> viewers{player.getNetworkIdentifier()};
        WorldEffectBeforeEvent event(getLevelFor(player), position, packet, viewers);
        worldSound.emit(event);
        if (!event.isCancelled())
            mNetworkHandler->sendToMany(viewers, packet, mCodecContext);
        return;
    }
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
    broadcastToSpawned(packet);
}


namespace {
    // in the world database, not under players/: a player of that name would save over the file
    const char *const WORLD_DYNAMIC_PROPERTIES_KEY = "falcon_world_dynamic_properties";
    const char *const LEGACY_WORLD_DYNAMIC_PROPERTIES = "world_dynamic_properties";
}

void ServerNetworkHandler::loadWorldDynamicProperties() {
    mWorldDynamicPropertiesUnreadable = false;
    Level &overworld = mWorlds.getDefault().getOverworld();
    Tag data;
    if (!overworld.loadSavedData(WORLD_DYNAMIC_PROPERTIES_KEY, data, false)) {
        if (overworld.hasSavedData(WORLD_DYNAMIC_PROPERTIES_KEY)) {
            mWorldDynamicPropertiesUnreadable = true;
            LOG_WARN(LogAreaID::Server, "The stored world dynamic properties are unreadable: kept as is, not saved");
            return;
        }
        // a player save in its place holds more than this one key: not ours, left to that player
        Tag legacy;
        if (!mPlayerData.loadData(LEGACY_WORLD_DYNAMIC_PROPERTIES, legacy) || !legacy.isCompound()
            || legacy.getKeys().size() != 1 || !legacy.contains("DynamicProperties"))
            return;
        if (!overworld.saveSavedData(WORLD_DYNAMIC_PROPERTIES_KEY, legacy, false))
            return;
        mPlayerData.removeData(LEGACY_WORLD_DYNAMIC_PROPERTIES);
        data = std::move(legacy);
    }

    const Tag *properties = data.get("DynamicProperties");
    if (properties != nullptr)
        deserializeDynamicProperties(*properties, mWorldDynamicProperties);
}

void ServerNetworkHandler::saveWorldDynamicProperties(bool checked) {
    Level &overworld = mWorlds.getDefault().getOverworld();
    if (!overworld.isStorageOpen() || mWorldDynamicPropertiesUnreadable)
        return;
    Tag data = Tag::ofCompound();
    data.put("DynamicProperties", serializeDynamicProperties(mWorldDynamicProperties));
    if (!overworld.saveSavedData(WORLD_DYNAMIC_PROPERTIES_KEY, data, false) && checked)
        throw std::runtime_error("Could not save world dynamic properties");
}


void ServerNetworkHandler::explodeAfterActors(Level &level, const Vector3f &center, double size,
                                              const ServerActor &source, bool fromTnt) {
    bool breakBlocks = true;
    double fireChance = 0;
    EventSignal<ActorExplodeBeforeEvent> &explode = mEventBus.before().mActorExplode;
    if (explode.hasSubscribers()) {
        ActorExplodeBeforeEvent event(level, source, center, size);
        explode.emit(event);
        if (event.isCancelled())
            return;
        size = event.mSize;
        breakBlocks = event.mBreakBlocks;
        fireChance = event.mFireChance;
    }

    if (mTickingActors) {
        const PendingExplosion pending{&level, center, size, source.getUniqueId(), fromTnt};
        if (breakBlocks && fireChance == 0)
            mPendingExplosions.push_back(pending);
        else
            mConfiguredExplosions.push_back({pending, breakBlocks, fireChance});
        return;
    }

    Explosion explosion(*this, level, center, size, &source, fromTnt);
    explosion.setFireChance(fireChance);
    if (breakBlocks)
        explosion.explode();
    else
        explosion.explodeWithoutBlocks();
}

namespace {
    const size_t CROWDED_EXPLOSIONS = 16;
    const size_t APART_EXPLOSIONS = 32;

    ThrownProjectileActor *heldAfterHit(ServerActor &actor) {
        ThrownProjectileActor *thrown = dynamic_cast<ThrownProjectileActor *>(&actor);
        return thrown != nullptr && thrown->isHeldAfterHit() ? thrown : nullptr;
    }
}

void ServerNetworkHandler::_flushExplosions() {
    for (const ConfiguredExplosion &configured: std::exchange(mConfiguredExplosions, {})) {
        const PendingExplosion &pending = configured.mExplosion;
        const auto source = mActors.find(pending.mSourceId);
        Explosion explosion(*this, *pending.mLevel, pending.mCenter, pending.mSize,
                            source == mActors.end() ? nullptr : source->second.get(), pending.mFromTnt);
        explosion.setFireChance(configured.mFireChance);
        if (configured.mBreakBlocks)
            explosion.explode();
        else
            explosion.explodeWithoutBlocks();
    }
    if (mPendingExplosions.empty())
        return;

    // the order within a tick is no one's to rely on: the explosions that break nothing (in water) go first,
    // over blocks that stay as they are, and share one read of them
    const auto quietEnd = std::stable_partition(
            mPendingExplosions.begin(), mPendingExplosions.end(), [this](const PendingExplosion &pending) {
                return mActors.find(pending.mSourceId) != mActors.end()
                       && (Explosion::breaksNothingAt(*pending.mLevel, pending.mCenter)
                           || (pending.mSize >= 0.1
                               && Explosion::isReachEmpty(*pending.mLevel, pending.mCenter, pending.mSize)));
            });
    const size_t quietCount = (size_t) (quietEnd - mPendingExplosions.begin());
    // neighbours one after the other: the read of the blocks around one serves the next
    const auto byPlace = [](const PendingExplosion &left, const PendingExplosion &right) {
        const int32_t leftX = (int32_t) std::floor(left.mCenter.x) >> 5;
        const int32_t rightX = (int32_t) std::floor(right.mCenter.x) >> 5;
        if (leftX != rightX)
            return leftX < rightX;
        const int32_t leftZ = (int32_t) std::floor(left.mCenter.z) >> 5;
        const int32_t rightZ = (int32_t) std::floor(right.mCenter.z) >> 5;
        if (leftZ != rightZ)
            return leftZ < rightZ;
        return ((int32_t) std::floor(left.mCenter.y) >> 4) < ((int32_t) std::floor(right.mCenter.y) >> 4);
    };
    // the others too: many of them go off in the air and leave the blocks as they were
    std::stable_sort(mPendingExplosions.begin(), quietEnd, byPlace);
    std::stable_sort(quietEnd, mPendingExplosions.end(), byPlace);

    // a block falling or an orb dropping out of one explosion joins the targets of the next ones
    mActorChanges.clear();
    mJoinedTracked = true;
    struct JoinedScope {
        bool &mTracked;
        ~JoinedScope() {
            mTracked = false;
        }
    } joinedScope{mJoinedTracked};

    // the sky light of a column an explosion digs into is worked out once, after the last explosion
    struct SkyLightScope {
        std::vector<Level *> mLevels;
        ~SkyLightScope() {
            for (Level *level: mLevels)
                level->endSkyLightBatch();
        }
    } skyLight;
    for (const PendingExplosion &pending: mPendingExplosions) {
        if (std::find(skyLight.mLevels.begin(), skyLight.mLevels.end(), pending.mLevel) == skyLight.mLevels.end()) {
            skyLight.mLevels.push_back(pending.mLevel);
            pending.mLevel->beginSkyLightBatch();
        }
    }

    ExplosionBatch batch(*this);
    batch.setFollowingJoins(true);

    // the ones that break nothing: plugins and scripts have their say on each, then every push is worked out
    // at once on every core, then each explosion ends in turn (damage, items, effects)
    {
        Level *quietLevel = quietCount == 0 ? nullptr : mPendingExplosions[0].mLevel;
        std::vector<std::unique_ptr<Explosion>> quiet;
        std::vector<ExplosionBatch::QuietBlast> blasts;
        std::vector<size_t> ranks;
        for (size_t index = 0; index < quietCount; ++index) {
            const PendingExplosion pending = mPendingExplosions[index];
            const auto source = mActors.find(pending.mSourceId);
            const ServerActor *sourceActor = source == mActors.end() ? nullptr : source->second.get();
            std::unique_ptr<Explosion> explosion(
                    new Explosion(*this, *pending.mLevel, pending.mCenter, pending.mSize, sourceActor, pending.mFromTnt));
            explosion->setBatch(&batch);
            if (!explosion->explodeA())
                continue;

            if (pending.mLevel == quietLevel && sourceActor != nullptr) {
                ranks.push_back(blasts.size());
                blasts.push_back(ExplosionBatch::QuietBlast{pending.mCenter, explosion->getSize(), sourceActor});
            } else {
                ranks.push_back(SIZE_MAX);
            }
            quiet.push_back(std::move(explosion));
        }
        if (quietLevel != nullptr)
            batch.settleQuiet(*quietLevel, blasts);
        for (size_t index = 0; index < quiet.size(); ++index) {
            if (ranks[index] != SIZE_MAX)
                quiet[index]->setSettledAs(ranks[index]);
            quiet[index]->explodeB();
        }
    }

    // the ones left break blocks, one after the other; when they are many their rays and their pushes are
    // shared out to helper threads kept awake until the last one
    batch.setCrowded(mPendingExplosions.size() - quietCount >= CROWDED_EXPLOSIONS);
    std::vector<Vector3f> centers;
    for (const PendingExplosion &pending: mPendingExplosions)
        centers.push_back(pending.mCenter);

    // when nothing can call an explosion off (no plugin, no script listening before it), the ones standing
    // apart are worked out together on every core while this thread ends them one by one
    size_t apartEnd = quietCount;
    if (mPendingExplosions.size() - quietCount >= APART_EXPLOSIONS
        && !mEventBus.before().mExplosion.hasSubscribers()
        && !mScriptEngine.hasSubscribers("before.explosion")) {
        batch.setCrowded(false);
        Level *apartLevel = mPendingExplosions[quietCount].mLevel;
        std::vector<std::unique_ptr<Explosion>> apart;
        std::vector<Explosion *> apartList;
        const size_t pendingEnd = mPendingExplosions.size();
        // the others first in the list, the ones of another level left for the loop below
        std::stable_partition(mPendingExplosions.begin() + (std::ptrdiff_t) quietCount, mPendingExplosions.end(),
                              [apartLevel](const PendingExplosion &pending) { return pending.mLevel == apartLevel; });
        for (size_t index = quietCount; index < pendingEnd && mPendingExplosions[index].mLevel == apartLevel; ++index) {
            const PendingExplosion pending = mPendingExplosions[index];
            const auto source = mActors.find(pending.mSourceId);
            apart.emplace_back(new Explosion(*this, *pending.mLevel, pending.mCenter, pending.mSize,
                                             source == mActors.end() ? nullptr : source->second.get(), pending.mFromTnt));
            apart.back()->setBatch(&batch);
            apartList.push_back(apart.back().get());
            apartEnd = index + 1;
        }
        batch.runApart(*apartLevel, apartList);
    }

    // by index: an explosion may queue another
    for (size_t index = apartEnd; index < mPendingExplosions.size(); ++index) {
        const PendingExplosion pending = mPendingExplosions[index];
        if (index < centers.size())
            batch.setUpcoming(centers.data() + index, centers.size() - index);
        else
            batch.setUpcoming(nullptr, 0);
        const auto source = mActors.find(pending.mSourceId);
        Explosion explosion(*this, *pending.mLevel, pending.mCenter, pending.mSize,
                            source == mActors.end() ? nullptr : source->second.get(), pending.mFromTnt);
        explosion.setBatch(&batch);
        explosion.explode();
    }
    mPendingExplosions.clear();
}

void ServerNetworkHandler::tickActors() {
    mActorIndexDirty = true;
    mTickingActors = true;
    // the moves of a crowd leave together at the end of the pass
    mNetworkHandler->cork();
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
        const MobActor *mob = dynamic_cast<const MobActor *>(&actor);
        if (mob != nullptr && mProperties.getDifficulty() == Difficulty::Peaceful && mob->shouldRemoveInPeaceful()) {
            expired.push_back(actorId);
            continue;
        }

        Level &level = getLevelFor(actor);
        actor.addLifetimeTick();
        if (actor.tickLimitedLife()) {
            expired.push_back(actorId);
            continue;
        }
        actor.tickCombat(1);

        if (actor.isDead()) {
            actor.addDeathTick();
            actor.tickDeath(*this);
            if (actor.getDeathTicks() >= actor.getDeathDuration() || actor.isExpired())
                expired.push_back(actorId);
            continue;
        }

        // a plugin moves, burns and hurts the actor it draws: blocks, freezing and effects stay out of it
        if (actor.isDrawnByPlugin()) {
            actor.tick(*this);
            continue;
        }

        if (dynamic_cast<ExperienceOrbActor *>(&actor) != nullptr || dynamic_cast<PaintingActor *>(&actor) != nullptr) {
            actor.tick(*this);
            if (actor.isExpired())
                expired.push_back(actorId);
            continue;
        }

        if (!actor.isProjectile()) {
            const ActorSize size = actor.getSize();
            // nothing hurts an invulnerable actor: where its eyes are is not even looked at
            if (!actor.isInvulnerable() && _isEyeInsideSolidBlock(level, actor.getPosition(), size.mHeight))
                actor.hurt(*this, ACTOR_SUFFOCATION_DAMAGE,
                           ActorDamageSource::environment("death.attack.inWall", actor.getName()));

            BlockContactSystem::tick(*this, actor);
            if (!actor.isAlive() || &getLevelFor(actor) != &level)
                continue;

            FreezingSystem::tick(*this, actor);
            if (!actor.isAlive())
                continue;
            actor.tickEffects(1);
            if (actor.refreshVisibleEffects())
                syncActorFlags(actor);
        }

        ProfilerScopedSection projectileSection(mProfiler, ProfilerSection::ActorProjectiles, actor.isProjectile());

        if (dynamic_cast<FireworksRocketActor *>(&actor) != nullptr) {
            ProjectileData &firework = actor.getProjectileData();

            ServerPlayer *rider = nullptr;
            if (firework.mFireworkAttached) {
                for (auto &playerEntry: mPlayers) {
                    if (playerEntry.second.getUniqueId() == actor.getOwnerUniqueId()
                        && playerEntry.second.isSpawned())
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

        if (ThrownProjectileActor *held = actor.isProjectile() ? heldAfterHit(actor) : nullptr) {
            held->tickAfterHit(*this);
            expired.push_back(actorId);
            continue;
        }

        if (actor.isProjectile() && actor.getProjectileData().mStuck) {
            if (tickPlantedProjectile(actor, level))
                expired.push_back(actorId);
            continue;
        }

        if (actor.isProjectile()) {
            Vector3f motion = actor.getMotion();
            ProjectileActor *steered = dynamic_cast<ProjectileActor *>(&actor);
            const ProjectileActor *projectile = steered;
            const ProjectileComponent &settings = actor.getProjectileComponent();
            const Vector3f &at = actor.getPosition();
            const int32_t atX = (int32_t) std::floor(at.x);
            const int32_t atY = (int32_t) std::floor(at.y);
            const int32_t atZ = (int32_t) std::floor(at.z);
            // liquid_inertia replaces inertia when the tick starts in water
            const bool inWater = LiquidView(level.getBlockState(atX, atY, atZ)).isWater()
                                 || LiquidView(level.getBlockStateAtLayer(atX, atY, atZ, 1)).isWater();
            const bool arrowLike = dynamic_cast<const ArrowActor *>(&actor) != nullptr;

            bool drifting = false;
            if (arrowLike && actor.getProjectileData().mReturning) {
                ServerPlayer *shooter = nullptr;
                for (auto &playerEntry: mPlayers) {
                    if (playerEntry.second.getUniqueId() == actor.getOwnerUniqueId()
                        && playerEntry.second.isSpawned() && !playerEntry.second.isDead())
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
                const float toY = shooterPosition.y + shooter->getEyeHeight() - actorPosition.y;
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
                // inertia and gravity wait for the move: a shot covers its full launch speed on its first tick
                drifting = steered == nullptr || !steered->steer(*this, motion);
            }

            const Vector3f previousPosition = actor.getPosition();

            Vector3f position = previousPosition;
            position.x += motion.x;
            position.y += motion.y;
            position.z += motion.z;

            // a script or plugin impulse can make it non finite: no block or actor lookup survives that
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                expired.push_back(actorId);
                continue;
            }

            const Vector3f startRotation = actor.getRotation();
            actor.setMotion(motion);
            actor.setPosition(position);
            if (steered != nullptr)
                steered->afterMove(*this);
            if (drifting)
                actor.setRotation(ProjectileSystem::face(startRotation, motion));
            else
                faceMotion(actor, motion);

            const int32_t blockX = (int32_t) std::floor(position.x);
            const int32_t blockY = (int32_t) std::floor(position.y);
            const int32_t blockZ = (int32_t) std::floor(position.z);

            if (arrowLike && actor.getProjectileData().mReturning) {
                actor.addLifetimeTick();
                broadcastActorMove(actor);
                continue;
            }

            if (actor.getLifetimeTicks() > 1 && blocksProjectile(level, actor, position, blockX, blockY, blockZ)) {
                const Vector3f hitPosition((float) blockX + 0.5f, (float) blockY + 0.5f, (float) blockZ + 0.5f);
                const Vector3i hitBlock(blockX, blockY, blockZ);
                if (!allowProjectileHit(*this, actor, level, hitPosition, nullptr, &hitBlock)) {
                    expired.push_back(actorId);
                    continue;
                }

                const BlockState hitState = level.getBlockState(blockX, blockY, blockZ);
                if (DetectorSystem::isTarget(hitState))
                    DetectorSystem::onTargetHit(*this, level, hitBlock, position, previousPosition, actor, arrowLike);
                const Block *block = VanillaBlocks::fromIdentifier(hitState.mName);
                if (block != nullptr && block->onProjectileHit(*this, level, hitBlock, hitState, actor)) {
                    expired.push_back(actorId);
                    continue;
                }

                if (!onThrownProjectileHit(actor, hitPosition, nullptr))
                    mScriptEngine.onProjectileHitBlock(actor, blockX, blockY, blockZ, previousPosition, position);

                if (actor.getProjectileData().mStuck) {
                    plantProjectile(actor, level, previousPosition, Vector3i(blockX, blockY, blockZ));
                    continue;
                }

                if (heldAfterHit(actor) != nullptr) {
                    broadcastActorMove(actor);
                    continue;
                }

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
                const int64_t sampleCount = std::max<int64_t>(
                        1, (int64_t) std::min(std::ceil((double) sweepLength / 0.25), 2147483647.0));

                ServerPlayer *hitPlayer = nullptr;
                ServerActor *hitActor = nullptr;
                Vector3f contactPosition = position;

                // candidates picked once per tick: only a box meeting the sweep's bounds can hold a sample
                const Vector3f low(std::min(previousPosition.x, position.x), std::min(previousPosition.y, position.y),
                                   std::min(previousPosition.z, position.z));
                const Vector3f high(std::max(previousPosition.x, position.x), std::max(previousPosition.y, position.y),
                                    std::max(previousPosition.z, position.z));
                const auto meetsSweep = [&low, &high](const AxisAlignedBB &box) {
                    return box.mMaxX >= low.x && box.mMinX <= high.x && box.mMaxY >= low.y && box.mMinY <= high.y
                           && box.mMaxZ >= low.z && box.mMinZ <= high.z;
                };

                std::vector<ServerPlayer *> nearPlayers;
                for (auto &playerEntry: mPlayers) {
                    ServerPlayer &candidate = playerEntry.second;
                    if (!candidate.isSpawned() || !candidate.sharesLevelWith(actor))
                        continue;
                    if (candidate.getUniqueId() == actor.getOwnerUniqueId() && actor.getLifetimeTicks() < 8)
                        continue;
                    if (meetsSweep(projectileHitBox(candidate.getPosition(), candidate.getBodyWidth(),
                                                    candidate.getBodyHeight())))
                        nearPlayers.push_back(&candidate);
                }

                std::vector<std::pair<ServerActor *, AxisAlignedBB>> nearActors;
                const auto consider = [&](ServerActor &candidate) {
                    if (&candidate == &actor || !candidate.isAlive() || !candidate.sharesLevelWith(actor))
                        return;

                    const bool candidateIsProjectile = candidate.isProjectile();
                    if (candidateIsProjectile && candidate.getProjectileData().mStuck)
                        return;
                    if (candidateIsProjectile && candidate.getOwnerUniqueId() == actor.getOwnerUniqueId()
                        && candidate.getLifetimeTicks() < 8)
                        return;
                    if (projectile != nullptr && projectile->passesThroughOwner()
                        && candidate.getUniqueId() == actor.getOwnerUniqueId())
                        return;

                    const ActorSize size = ActorClassRegistry::getSize(candidate.getTypeId());
                    const auto *painting = dynamic_cast<const PaintingActor *>(&candidate);
                    const auto box = painting == nullptr
                                             ? projectileHitBox(candidate.getPosition(), size.mWidth, size.mHeight)
                                             : painting->getPaintingBounds().expand(PROJECTILE_HIT_GROW,
                                                                                   PROJECTILE_HIT_GROW,
                                                                                   PROJECTILE_HIT_GROW);
                    if (meetsSweep(box))
                        nearActors.emplace_back(&candidate, box);
                };
                // a sweep over many columns reads every actor once rather than every column of the index
                if (high.x - low.x <= SWEEP_INDEX_SPAN && high.z - low.z <= SWEEP_INDEX_SPAN) {
                    _refreshActorIndex();
                    const Vector3f middle((low.x + high.x) * 0.5f, position.y, (low.z + high.z) * 0.5f);
                    const float reach = std::max(high.x - low.x, high.z - low.z) * 0.5f
                                        + std::max(mActorIndexWidestHalf, mActorIndexWidestBox) + PROJECTILE_HIT_GROW;
                    forEachActorNear(middle, reach, consider);
                } else {
                    for (auto &actorEntry: mActors)
                        consider(*actorEntry.second);
                }

                // the first 0.25 block sample inside a box, from the segment's entry into it; players first on a tie
                const auto sampleAt = [&](int64_t sample) {
                    const float progress = (float) ((double) sample / (double) sampleCount);
                    return Vector3f(previousPosition.x + sweepX * progress, previousPosition.y + sweepY * progress,
                                    previousPosition.z + sweepZ * progress);
                };
                const auto firstSample = [&](const AxisAlignedBB &box) -> int64_t {
                    double enter = 0.0, leave = 1.0;
                    const float from[3] = {previousPosition.x, previousPosition.y, previousPosition.z};
                    const float delta[3] = {sweepX, sweepY, sweepZ};
                    const float lows[3] = {box.mMinX, box.mMinY, box.mMinZ};
                    const float highs[3] = {box.mMaxX, box.mMaxY, box.mMaxZ};
                    for (int axis = 0; axis < 3; ++axis) {
                        if (delta[axis] == 0.0f) {
                            if (from[axis] < lows[axis] || from[axis] > highs[axis])
                                return 0;
                            continue;
                        }
                        double slabIn = ((double) lows[axis] - from[axis]) / delta[axis];
                        double slabOut = ((double) highs[axis] - from[axis]) / delta[axis];
                        if (slabIn > slabOut)
                            std::swap(slabIn, slabOut);
                        enter = std::max(enter, slabIn);
                        leave = std::min(leave, slabOut);
                        if (enter > leave)
                            return 0;
                    }
                    const int64_t first = std::max<int64_t>(1, (int64_t) std::ceil(enter * (double) sampleCount));
                    // float rounding at the box edge: the exact test decides between the nearest samples
                    for (int64_t sample = std::max<int64_t>(1, first - 1); sample <= first + 1 && sample <= sampleCount;
                         ++sample) {
                        const auto point = sampleAt(sample);
                        if (point.x >= box.mMinX && point.x <= box.mMaxX && point.y >= box.mMinY && point.y <= box.mMaxY
                            && point.z >= box.mMinZ && point.z <= box.mMaxZ)
                            return sample;
                    }
                    return 0;
                };

                int64_t hitSample = 0;
                for (ServerPlayer *candidate: nearPlayers) {
                    const int64_t sample = firstSample(projectileHitBox(candidate->getPosition(), candidate->getBodyWidth(),
                                                                       candidate->getBodyHeight()));
                    if (sample != 0 && (hitSample == 0 || sample < hitSample)) {
                        hitSample = sample;
                        hitPlayer = candidate;
                    }
                }
                for (const auto &candidate: nearActors) {
                    const int64_t sample = firstSample(candidate.second);
                    if (sample != 0 && (hitSample == 0 || sample < hitSample)) {
                        hitSample = sample;
                        hitPlayer = nullptr;
                        hitActor = candidate.first;
                    }
                }
                if (hitSample != 0)
                    contactPosition = sampleAt(hitSample);

                if (hitPlayer != nullptr) {
                    if (!allowProjectileHit(*this, actor, level, contactPosition, hitPlayer, nullptr)) {
                        expired.push_back(actorId);
                        continue;
                    }

                    mScriptEngine.onProjectileHitEntity(actor, *hitPlayer, contactPosition);
                    onThrownProjectileHit(actor, contactPosition, hitPlayer);
                    if (heldAfterHit(actor) != nullptr)
                        broadcastActorMove(actor);
                    else
                        expired.push_back(actorId);
                    continue;
                }

                // the game asks the actor first: one that teleports out of the way lets the projectile fly on
                if (hitActor != nullptr && hitActor->dodgeProjectile(*this, actor))
                    hitActor = nullptr;

                if (hitActor != nullptr && ProjectileReflection::reflect(*this, level, actor, *hitActor)) {
                    // the game stops it where it met the entity, then turns, slows and drops the new motion this tick
                    actor.setPosition(contactPosition);
                    if (drifting) {
                        actor.setRotation(ProjectileSystem::face(startRotation, actor.getMotion()));
                        actor.setMotion(ProjectileSystem::dragAndFall(actor.getMotion(), settings, inWater));
                    }
                    broadcastActorMove(actor);
                    continue;
                }

                if (hitActor != nullptr) {
                    if (!allowProjectileHit(*this, actor, level, contactPosition, hitActor, nullptr)) {
                        expired.push_back(actorId);
                        continue;
                    }

                    mScriptEngine.onProjectileHitEntity(actor, *hitActor, contactPosition);
                    onThrownProjectileHitActor(actor, contactPosition, *hitActor);
                    if (heldAfterHit(actor) != nullptr)
                        broadcastActorMove(actor);
                    else
                        expired.push_back(actorId);
                    continue;
                }
            }

            if (drifting)
                actor.setMotion(ProjectileSystem::dragAndFall(motion, settings, inWater));

            MoveActorAbsolutePacket move;
            move.mRuntimeActorId = (int64_t) actor.getRuntimeId();
            move.mPosition = position;
            move.mRotation = actor.getRotation();

            for (auto &playerEntry: mPlayers) {
                if (playerEntry.second.isSpawned() && playerEntry.second.sharesLevelWith(actor))
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
    // ids first: an effect can kill, and the death's script events can spawn another cloud
    std::vector<int64_t> cloudIds;
    cloudIds.reserve(mLingeringClouds.size());
    for (const auto &entry: mLingeringClouds)
        cloudIds.push_back(entry.first);
    for (const int64_t cloudId: cloudIds) {
        const auto found = mLingeringClouds.find(cloudId);
        if (found == mLingeringClouds.end())
            continue;
        LingeringCloud &cloud = found->second;
        cloud.mAge += 1;

        if (mActors.find(cloudId) == mActors.end() || cloud.mAge > cloud.mWaitTime + cloud.mDuration) {
            expiredClouds.push_back(cloudId);
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
            std::vector<uint64_t> insidePlayers;

            for (auto &playerEntry: mPlayers) {
                ServerPlayer &nearby = playerEntry.second;
                if (!nearby.isSpawned() || nearby.getDimension() != cloud.mDimension
                    || nearby.getWorldId() != cloud.mWorldId
                    || nearby.getUniqueId() == cloud.mOwnerUniqueId)
                    continue;

                const Vector3f position = nearby.getPosition();
                const float dx = position.x - cloud.mPosition.x;
                const float dy = position.y - cloud.mPosition.y;
                const float dz = position.z - cloud.mPosition.z;
                if (dx * dx + dz * dz > radiusSquared || std::fabs(dy) > 1.0f)
                    continue;

                insidePlayers.push_back(nearby.getRuntimeId());
            }

            std::vector<int64_t> inside;
            for (auto &actorEntry: mActors) {
                ServerActor &nearby = *actorEntry.second;
                if (actorEntry.first == cloudId || !nearby.isAlive() || nearby.isProjectile() || nearby.isDead()
                    || nearby.getDimension() != cloud.mDimension || nearby.getWorldId() != cloud.mWorldId
                    || nearby.getUniqueId() == cloud.mOwnerUniqueId)
                    continue;

                const Vector3f position = nearby.getPosition();
                const float dx = position.x - cloud.mPosition.x;
                const float dy = position.y - cloud.mPosition.y;
                const float dz = position.z - cloud.mPosition.z;
                if (dx * dx + dz * dz > radiusSquared || std::fabs(dy) > 1.0f)
                    continue;

                inside.push_back(actorEntry.first);
            }
            if (!insidePlayers.empty() || !inside.empty()) {
                EventSignal<AreaEffectCloudApplyBeforeEvent> &apply = mEventBus.before().mAreaEffectCloudApply;
                if (apply.hasSubscribers()) {
                    ServerActor *cloudActor = getActor(cloudId);
                    if (cloudActor == nullptr)
                        continue;
                    std::vector<MobEffectInstance> effects;
                    if (cloud.mEffects) {
                        effects = *cloud.mEffects;
                    } else {
                        for (const auto &effect: getPotionEffects(cloud.mPotionId))
                            effects.push_back(potionEffectInstance(effect, 0.25f));
                    }

                    AreaEffectCloudApplyBeforeEvent event(
                            *cloudActor, getLevelFor(*cloudActor), insidePlayers, inside, std::move(effects),
                            cloud.mAge, cloud.mWaitTime + cloud.mDuration, cloud.mRadius, cloud.mRadiusOnUse,
                            cloud.mRadiusPerTick, cloud.mReapplicationDelay);
                    apply.emit(event);
                    if (getActor(cloudId) == nullptr)
                        continue;
                    if (event.mEffectsChanged)
                        cloud.mEffects = std::move(event.mEffects);
                    if (event.isCancelled()) {
                        insidePlayers.clear();
                        inside.clear();
                    }
                }
            }
            for (const uint64_t id: insidePlayers) {
                if (ServerPlayer *nearby = getPlayerByRuntimeId(id)) {
                    if (cloud.mEffects)
                        _applyCloudEffects(*nearby, *cloud.mEffects);
                    else
                        applyPotionEffects(*nearby, cloud.mPotionId, 0.25f);
                    touched = true;
                }
            }

            // applied after the walk: an instant damage can kill and remove the actor
            for (const int64_t uniqueId: inside) {
                if (ServerActor *nearby = getActor(uniqueId)) {
                    if (cloud.mEffects)
                        _applyCloudEffects(*nearby, *cloud.mEffects);
                    else
                        applyPotionEffects(*nearby, cloud.mPotionId, 0.25f);
                    touched = true;
                }
            }

            if (touched) {
                cloud.mRadius += cloud.mRadiusOnUse;
                cloud.mRadiusOnUse *= 0.5f;
            }
        }

        if (cloud.mRadius <= LINGERING_CLOUD_MIN_RADIUS) {
            expiredClouds.push_back(cloudId);
            continue;
        }

        if (cloud.mAge % 10 == 0) {
            ServerActor *cloudActor = getActor(cloudId);
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

    mTickingActors = false;
    // the block updates and drops of the explosions leave with the moves, in one go
    _flushExplosions();
    mNetworkHandler->uncork();

    for (const int64_t uniqueId: expiredClouds) {
        mLingeringClouds.erase(uniqueId);
        removeActor(uniqueId);
    }

    for (const int64_t uniqueId: expired)
        removeActor(uniqueId);

    for (const int64_t uniqueId: mDetachedActors)
        _unregisterActor(uniqueId);
    mDetachedActors.clear();
}
