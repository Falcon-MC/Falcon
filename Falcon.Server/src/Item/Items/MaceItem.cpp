#include "Item/Items/MaceItem.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Core/Math/MathConstants.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemStack.h"

#include <algorithm>
#include <cmath>

namespace {
    const float SMASH_MINIMUM_FALL = 1.5f;
    const float SMASH_PARTICLE_DAMAGE = 7.0f;
    const float SMASH_HEAVY_DAMAGE = 16.0f;
    const float WIND_BURST_RADIUS = 2.5f;
    const float WIND_BURST_MAX_FALL = 7.5f;
    const float PLAYER_EYE_HEIGHT = 1.62f;
    const char *WIND_BURST_PARTICLE = "minecraft:wind_explosion_emitter";

    float rise(const Actor &actor) {
        return actor.getHighestPosition() - actor.getPosition().y;
    }

    void gustActor(ServerNetworkHandler &owner, Actor &target, const Vector3f &origin, int32_t level) {
        const Vector3f position = target.getPosition();
        Vector3f direction(position.x - origin.x, position.y - origin.y, position.z - origin.z);
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                       direction.z * direction.z);
        if (length <= 0.0f || length > WIND_BURST_RADIUS)
            return;

        direction.x /= length;
        direction.y /= length;
        direction.z /= length;

        const float strength = 0.6f + 0.15f * (float) level;
        Vector3f motion = target.getMotion();
        motion.x += direction.x * strength;
        motion.y += direction.y * strength;
        motion.z += direction.z * strength;
        motion.y = std::max(motion.y, 0.4f + 0.1f * (float) level);

        target.setMotion(motion);
        owner.sendActorMotion(target);
        owner.spawnParticleEffect(owner.getLevelFor(target), WIND_BURST_PARTICLE,
                                  Vector3f(position.x, position.y + PLAYER_EYE_HEIGHT * 0.6f, position.z));
    }
}

MaceItem::MaceItem(const Item &base) : Item(base) {
}

int32_t MaceItem::computeSmashDamage(int32_t fallHeight) {
    if ((float) fallHeight < SMASH_MINIMUM_FALL)
        return BASE_ATTACK_DAMAGE;

    int32_t damage = 0;
    for (int32_t i = 0; i <= fallHeight; ++i) {
        if (i < 3)
            damage += 4;
        else if (i < 8)
            damage += 2;
        else
            damage += 1;
    }

    return damage;
}

float MaceItem::getAttackDamageBonus(const ItemStack &item, ServerPlayer &attacker) const {
    const float height = rise(attacker);
    const int32_t flooredHeight = (int32_t) std::floor(height);
    attacker.updateFallDistance();

    float bonus = (float) computeSmashDamage(flooredHeight) - (float) BASE_ATTACK_DAMAGE;

    const int32_t density = ItemEnchantments::getLevel(item, EnchantmentIds::DENSITY);
    if (density > 0 && height >= SMASH_MINIMUM_FALL)
        bonus += height * 0.5f * (float) density;

    if ((float) flooredHeight >= SMASH_MINIMUM_FALL)
        attacker.resetFallDistance();

    return bonus;
}

float MaceItem::getArmorEfficiency(const ItemStack &item) const {
    const int32_t breach = ItemEnchantments::getLevel(item, EnchantmentIds::BREACH);
    if (breach <= 0)
        return 1.0f;

    return (float) (100 - breach * 15) / 100.0f;
}

void MaceItem::onPostAttack(ServerNetworkHandler &owner, ServerPlayer &attacker, Actor &victim, float damage,
                            const ItemStack &item) const {
    if (damage >= SMASH_PARTICLE_DAMAGE) {
        const Vector3f position = victim.getPosition();

        LevelEventPacket particle;
        particle.mEventId = LevelEventPacket::Event::ParticleSmashAttackGroundDust;
        particle.mPosition = position;
        particle.mData = 0;
        Level &world = owner.getLevelFor(attacker);
        BlockActionHandler::broadcastToViewers(owner, world, position, particle);

        owner.playLevelSound(world,
                             damage >= SMASH_HEAVY_DAMAGE ? LevelSoundEvent::MACE_HEAVY_SMASH_GROUND
                                                          : LevelSoundEvent::MACE_SMASH_GROUND,
                             position);
    }

    const int32_t windBurst = ItemEnchantments::getLevel(item, EnchantmentIds::WIND_BURST);
    if (windBurst > 0)
        applyWindBurst(owner, attacker, windBurst);
}

void MaceItem::applyWindBurst(ServerNetworkHandler &owner, ServerPlayer &attacker, int32_t level) {
    const Vector3f origin = attacker.getPosition();
    const Vector3f attackerMotion = attacker.getMotion();

    float fallDistance = std::max(attacker.getFallDistance(), rise(attacker));
    if (fallDistance < SMASH_MINIMUM_FALL && attackerMotion.y < -0.08f)
        fallDistance = std::max(fallDistance, std::min(2.5f, -attackerMotion.y * 4.0f));

    if (fallDistance < SMASH_MINIMUM_FALL)
        return;

    const float clampedFall = std::min(fallDistance, WIND_BURST_MAX_FALL);
    float verticalBoost = 0.72f + clampedFall * 0.10f;
    if (level == 2)
        verticalBoost += 0.55f;
    else if (level >= 3)
        verticalBoost += 1.3f;

    const float yaw = attacker.getRotation().y * MathConstants::PI_F / 180.0f;
    const float forwardBoost = 0.08f + 0.02f * (float) level;

    Vector3f motion(-std::sin(yaw) * forwardBoost, verticalBoost, std::cos(yaw) * forwardBoost);
    attacker.resetFallDistance();
    attacker.setMotion(motion);
    owner.sendActorMotion(attacker);

    for (auto &entry: owner.getActors()) {
        ServerActor &target = *entry.second;
        if (!target.isAlive() || target.isProjectile() || target.getDimension() != attacker.getDimension())
            continue;

        gustActor(owner, target, origin, level);
    }

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &target = entry.second;
        if (&target == &attacker || !target.isSpawned() || target.isDead() ||
            target.getDimension() != attacker.getDimension())
            continue;

        gustActor(owner, target, origin, level);
    }

    Level &world = owner.getLevelFor(attacker);
    owner.spawnParticleEffect(world, WIND_BURST_PARTICLE,
                              Vector3f(origin.x, origin.y + PLAYER_EYE_HEIGHT * 0.6f, origin.z));
    owner.playLevelSound(world, LevelSoundEvent::MACE_SMASH_AIR, origin);
}
