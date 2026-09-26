#include "Item/Items/SpearItem.h"

#include "Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(SpearItem, 80);

#include "Actor/ActorClassRegistry.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Core/Math/MathConstants.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    const float MINIMUM_SPEED = 0.13f;
    const float MINIMUM_LUNGE_FOOD = 6.0f;
    const float BASE_LUNGE_EXHAUST = 4.0f;
    const int32_t STAB_COOLDOWN_TICKS = 20;
    const int32_t SWEEP_INTERVAL_TICKS = 5;
    const float MAX_STAB_DISTANCE = 5.0f;
    const float MINIMUM_AIM_DOT = 0.866f;
    const float SWEEP_REACH = 1.5f;
    const float SWEEP_EXPAND_HORIZONTAL = 1.5f;
    const float SWEEP_EXPAND_VERTICAL = 1.0f;
    const float PLAYER_EYE_HEIGHT = 1.62f;
    const float PLAYER_WIDTH = 0.6f;
    const float PLAYER_HEIGHT = 1.8f;

    float eyeHeightOf(float height) {
        return std::min(height * 0.5f + 0.1f, height);
    }

    const float BASE_MOVEMENT_SPEED = 0.1f;
    const float SPRINT_SPEED_MULTIPLIER = 1.3f;

    float movementSpeedOf(const ServerPlayer &player) {
        float speed = BASE_MOVEMENT_SPEED * player.getMovementSpeedMultiplier();
        if (player.getFlags().get(ActorFlag::Sprinting))
            speed *= SPRINT_SPEED_MULTIPLIER;
        return speed;
    }

    Vector3f lookDirectionOf(const ServerPlayer &player) {
        const Vector3f rotation = player.getRotation();
        const float pitch = rotation.x * MathConstants::DEGREES_TO_RADIANS_F;
        const float yaw = rotation.y * MathConstants::DEGREES_TO_RADIANS_F;

        return Vector3f(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    }

    std::string tierNameOf(const std::string &identifier) {
        const size_t colon = identifier.find(':');
        return colon == std::string::npos ? identifier : identifier.substr(colon + 1);
    }
}

SpearItem::SpearItem(const Item &base) : Item(base), mTierName(tierNameOf(base.getIdentifier())) {}

bool SpearItem::matches(const std::string &identifier) {
    static const std::string suffix = "_spear";
    return identifier.size() > suffix.size() &&
           identifier.compare(identifier.size() - suffix.size(), suffix.size(), suffix) == 0;
}

float SpearItem::getJabDamage(const ItemStack &item) const {
    const int32_t lungeLevel = ItemEnchantments::getLevel(item, EnchantmentIds::LUNGE);
    return (float) getAttackDamage() + (float) lungeLevel * 1.5f;
}

Actor *SpearItem::findTarget(ServerNetworkHandler &owner, ServerPlayer &player, float maxDistance) const {
    const Vector3f playerPosition = player.getPosition();
    const Vector3f eyePosition(playerPosition.x, playerPosition.y + PLAYER_EYE_HEIGHT, playerPosition.z);
    const Vector3f direction = lookDirectionOf(player);

    Actor *best = nullptr;
    float bestScore = -1.0f;

    const auto consider = [&](Actor &candidate, float width, float height) {
        (void) width;
        if (candidate.getDimension() != player.getDimension())
            return;

        const Vector3f candidatePosition = candidate.getPosition();
        const Vector3f target(candidatePosition.x, candidatePosition.y + eyeHeightOf(height) * 0.5f,
                              candidatePosition.z);

        const float dx = target.x - eyePosition.x;
        const float dy = target.y - eyePosition.y;
        const float dz = target.z - eyePosition.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance <= 0.0f || distance > maxDistance)
            return;

        const float dot = (direction.x * dx + direction.y * dy + direction.z * dz) / distance;
        if (dot < MINIMUM_AIM_DOT)
            return;

        const float score = dot - (distance / maxDistance) * 0.1f;
        if (score <= bestScore)
            return;

        bestScore = score;
        best = &candidate;
    };

    for (auto &entry: owner.getActors()) {
        ServerActor &candidate = *entry.second;
        if (!candidate.isAlive() || candidate.isDead() || candidate.isProjectile())
            continue;

        const ActorSize size = candidate.getSize();
        consider(candidate, size.mWidth, size.mHeight);
    }

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &candidate = entry.second;
        if (&candidate == &player || !candidate.isSpawned() || candidate.isDead())
            continue;
        if (candidate.getGameType() == (int32_t) GameType::Creative
            || candidate.getGameType() == (int32_t) GameType::Spectator)
            continue;

        consider(candidate, PLAYER_WIDTH, PLAYER_HEIGHT);
    }

    return best;
}

Actor *SpearItem::findSweepTarget(ServerNetworkHandler &owner, ServerPlayer &player) const {
    const Vector3f direction = lookDirectionOf(player);
    const Vector3f playerPosition = player.getPosition();

    const float minX = playerPosition.x - PLAYER_WIDTH * 0.5f - SWEEP_EXPAND_HORIZONTAL + direction.x * SWEEP_REACH;
    const float maxX = playerPosition.x + PLAYER_WIDTH * 0.5f + SWEEP_EXPAND_HORIZONTAL + direction.x * SWEEP_REACH;
    const float minY = playerPosition.y - SWEEP_EXPAND_VERTICAL + direction.y * SWEEP_REACH;
    const float maxY = playerPosition.y + PLAYER_HEIGHT + SWEEP_EXPAND_VERTICAL + direction.y * SWEEP_REACH;
    const float minZ = playerPosition.z - PLAYER_WIDTH * 0.5f - SWEEP_EXPAND_HORIZONTAL + direction.z * SWEEP_REACH;
    const float maxZ = playerPosition.z + PLAYER_WIDTH * 0.5f + SWEEP_EXPAND_HORIZONTAL + direction.z * SWEEP_REACH;

    Actor *best = nullptr;
    float closestDistance = std::numeric_limits<float>::max();

    const auto consider = [&](Actor &candidate, float width, float height) {
        if (candidate.getDimension() != player.getDimension())
            return;

        const Vector3f candidatePosition = candidate.getPosition();
        const float half = width * 0.5f;
        if (candidatePosition.x + half < minX || candidatePosition.x - half > maxX)
            return;
        if (candidatePosition.y + height < minY || candidatePosition.y > maxY)
            return;
        if (candidatePosition.z + half < minZ || candidatePosition.z - half > maxZ)
            return;

        const float dx = candidatePosition.x - playerPosition.x;
        const float dy = candidatePosition.y - playerPosition.y;
        const float dz = candidatePosition.z - playerPosition.z;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (distance >= closestDistance)
            return;

        closestDistance = distance;
        best = &candidate;
    };

    for (auto &entry: owner.getActors()) {
        ServerActor &candidate = *entry.second;
        if (!candidate.isAlive() || candidate.isDead() || candidate.isProjectile())
            continue;

        const ActorSize size = candidate.getSize();
        consider(candidate, size.mWidth, size.mHeight);
    }

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &candidate = entry.second;
        if (&candidate == &player || !candidate.isSpawned() || candidate.isDead())
            continue;
        if (candidate.getGameType() == (int32_t) GameType::Creative
            || candidate.getGameType() == (int32_t) GameType::Spectator)
            continue;

        consider(candidate, PLAYER_WIDTH, PLAYER_HEIGHT);
    }

    return best;
}

void SpearItem::applySpearDamage(ServerNetworkHandler &owner, ServerPlayer &attacker, Actor &target,
                                 float damage) const {
    ServerPlayer *victim = dynamic_cast<ServerPlayer *>(&target);
    if (victim != nullptr) {
        owner.hurt(*victim, damage,
                   DamageSource::attack("death.attack.player", victim->getName(), attacker, attacker.getName(),
                                        attacker.getPosition())
                           .withoutArmor()
                           .withoutCooldown());
        return;
    }

    ServerActor *actor = dynamic_cast<ServerActor *>(&target);
    if (actor != nullptr)
        owner.damageActor(*actor, damage,
                          DamageSource::attack("death.attack.player", actor->getName(), attacker, attacker.getName(),
                                               attacker.getPosition())
                                  .withoutArmor());
}

void SpearItem::applyLunge(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    const int32_t lungeLevel = ItemEnchantments::getLevel(item, EnchantmentIds::LUNGE);
    if (lungeLevel <= 0)
        return;

    if (player.getFlags().get(ActorFlag::Swimming) || player.getFlags().get(ActorFlag::Gliding))
        return;

    if (LiquidBlocksFetch::at(owner.getLevelFor(player), player.getPosition()).water)
        return;

    const int32_t gameType = player.getGameType();
    const bool finiteResources = gameType == (int32_t) GameType::Survival
                                 || gameType == (int32_t) GameType::Adventure;
    if (finiteResources && player.getFood() < MINIMUM_LUNGE_FOOD)
        return;

    const Vector3f direction = lookDirectionOf(player);
    const float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (length <= 0.0f)
        return;

    const float push = 0.5f + (float) lungeLevel * 0.4f;

    Vector3f motion = player.getMotion();
    motion.x += (direction.x / length) * push;
    motion.z += (direction.z / length) * push;
    player.setMotion(motion);
    owner.sendActorMotion(player);

    const char *sound = lungeLevel >= 3 ? LevelSoundEvent::LUNGE_3
                                        : (lungeLevel == 2 ? LevelSoundEvent::LUNGE_2 : LevelSoundEvent::LUNGE_1);
    owner.playLevelSound(owner.getLevelFor(player), sound, player.getPosition(), "minecraft:player");

    player.exhaust(BASE_LUNGE_EXHAUST * (float) lungeLevel);
}

bool SpearItem::onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    owner.playLevelSound(owner.getLevelFor(player), "item." + mTierName + ".use", player.getPosition(),
                         "minecraft:player");
    stab(owner, player, item);

    // Staying in the using state is what drives the sweep in onUsingTick.
    return true;
}

void SpearItem::stab(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    if (player.hasItemCooldown(item, owner.getCurrentTick()))
        return;

    player.startItemCooldown(item, owner.getCurrentTick(), STAB_COOLDOWN_TICKS);
    applyLunge(owner, player, item);

    Level &level = owner.getLevelFor(player);
    if (movementSpeedOf(player) < MINIMUM_SPEED || !player.getFlags().get(ActorFlag::Sprinting)) {
        owner.playLevelSound(level, "item." + mTierName + ".attack_miss", player.getPosition(), "minecraft:player");
        return;
    }

    Actor *target = findTarget(owner, player, MAX_STAB_DISTANCE);
    if (target != nullptr) {
        applySpearDamage(owner, player, *target, getJabDamage(item));
        owner.playLevelSound(level, "item." + mTierName + ".attack_hit", player.getPosition(), "minecraft:player");
    } else {
        owner.playLevelSound(level, "item." + mTierName + ".attack_miss", player.getPosition(), "minecraft:player");
    }

    owner.damagePlayerHeldItem(player, 1);
}

void SpearItem::onUsingTick(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                            int32_t elapsedTicks) const {
    if (elapsedTicks <= 0 || elapsedTicks % SWEEP_INTERVAL_TICKS != 0)
        return;

    const float speed = movementSpeedOf(player);
    if (speed < MINIMUM_SPEED)
        return;

    Actor *target = findSweepTarget(owner, player);
    if (target == nullptr)
        return;

    const float damage = (float) getAttackDamage() * 1.5f + speed * 3.0f;
    applySpearDamage(owner, player, *target, damage);
    owner.playLevelSound(owner.getLevelFor(player), "item." + mTierName + ".attack_hit", player.getPosition(),
                         "minecraft:player");
}

bool SpearItem::onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                            int32_t elapsedTicks) const {
    (void) owner;
    (void) player;
    (void) item;
    (void) elapsedTicks;

    // The jab already ran when the spear was raised, releasing it only ends the sweep.
    return true;
}
