#include "Item/Items/FireworkRocketItem.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Core/Math/MathConstants.h"
#include "Item/Items/ElytraItem.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>
#include <random>
#include <utility>

namespace {
    const char *FIREWORK_ACTOR = "minecraft:fireworks_rocket";
    const float ELYTRA_BOOST_SPEED = 2.0f;
    const char *FIREWORKS_TAG = "Fireworks";
    const char *EXPLOSIONS_TAG = "Explosions";
    const char *FLIGHT_TAG = "Flight";
    const char *FIREWORK_COLOR_TAG = "FireworkColor";
    const char *FIREWORK_FADE_TAG = "FireworkFade";
    const char *FIREWORK_FLICKER_TAG = "FireworkFlicker";
    const char *FIREWORK_TRAIL_TAG = "FireworkTrail";
    const char *FIREWORK_TYPE_TAG = "FireworkType";
    const int8_t DEFAULT_FIREWORK_COLOR = 0;
    const int8_t DEFAULT_FIREWORK_TYPE = 3;

    Tag makeDefaultExplosion() {
        Tag explosion = Tag::ofCompound();
        explosion.put(FIREWORK_COLOR_TAG, Tag::ofByteArray({DEFAULT_FIREWORK_COLOR}));
        explosion.put(FIREWORK_FADE_TAG, Tag::ofByteArray({}));
        explosion.putByte(FIREWORK_FLICKER_TAG, 0);
        explosion.putByte(FIREWORK_TRAIL_TAG, 0);
        explosion.putByte(FIREWORK_TYPE_TAG, DEFAULT_FIREWORK_TYPE);
        return explosion;
    }

    Tag buildFireworkData(const ItemStack &item) {
        if (item.mTag.getType() == Tag::Type::Compound) {
            const Tag *existing = item.mTag.get(FIREWORKS_TAG);
            if (existing != nullptr && existing->getType() == Tag::Type::Compound) {
                Tag data = Tag::ofCompound();
                data.put(FIREWORKS_TAG, *existing);
                return data;
            }
        }

        Tag fireworks = Tag::ofCompound();
        fireworks.put(EXPLOSIONS_TAG, Tag::ofList(Tag::Type::Compound, {makeDefaultExplosion()}));
        fireworks.putByte(FLIGHT_TAG, 1);

        Tag data = Tag::ofCompound();
        data.put(FIREWORKS_TAG, std::move(fireworks));
        return data;
    }

    int32_t rollLifetime() {
        static std::mt19937 generator{std::random_device{}()};
        std::uniform_int_distribution<int32_t> extra(0, 11);
        return 30 + extra(generator);
    }

    float gaussianDrift() {
        static std::mt19937 generator{std::random_device{}()};
        std::normal_distribution<float> distribution(0.0f, 1.0f);
        return distribution(generator) * 0.001f;
    }
}

bool isFireworkRocketActor(const std::string &identifier) {
    return identifier == FIREWORK_ACTOR;
}

FireworkRocketItem::FireworkRocketItem(const Item &base) : Item(base) {
}

bool FireworkRocketItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) item;

    if (!player.getFlags().get(ActorFlag::Gliding) || !ElytraItem::isWearingElytra(player))
        return false;

    const Vector3f rotation = player.getRotation();
    const float pitch = rotation.x * MathConstants::DEGREES_TO_RADIANS_F;
    const float yaw = rotation.y * MathConstants::DEGREES_TO_RADIANS_F;

    player.setMotion(Vector3f(-std::sin(yaw) * std::cos(pitch) * ELYTRA_BOOST_SPEED,
                              -std::sin(pitch) * ELYTRA_BOOST_SPEED,
                              std::cos(yaw) * std::cos(pitch) * ELYTRA_BOOST_SPEED));
    owner.sendActorMotion(player);

    ServerActor *rocket = owner.spawnActor(owner.getLevelFor(player), FIREWORK_ACTOR, player.getPosition());
    if (rocket != nullptr) {
        rocket->setProjectile(true);
        rocket->setOwnerUniqueId((int64_t) player.getRuntimeId());
        rocket->getProjectileData().mFireworkLifetime = rollLifetime();
        rocket->getProjectileData().mFireworkData = buildFireworkData(item);
        owner.syncActorFirework(*rocket);
        rocket->getProjectileData().mFireworkAttached = true;
    }

    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::LAUNCH, player.getPosition(),
                         "minecraft:player");
    player.consumeOneHeldItem();
    return true;
}

bool FireworkRocketItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                      const Vector3i &blockPosition, int32_t face,
                                      const Vector3f &clickPosition) const {
    (void) item;
    (void) clickPosition;

    if (player.getGameType() == (int32_t) GameType::Adventure)
        return false;

    Vector3f spawnPosition((float) blockPosition.x + 0.5f, (float) blockPosition.y + 0.5f,
                           (float) blockPosition.z + 0.5f);
    switch (face) {
        case 0:
            spawnPosition.y -= 1.0f;
            break;
        case 1:
            spawnPosition.y += 1.0f;
            break;
        case 2:
            spawnPosition.z -= 1.0f;
            break;
        case 3:
            spawnPosition.z += 1.0f;
            break;
        case 4:
            spawnPosition.x -= 1.0f;
            break;
        case 5:
            spawnPosition.x += 1.0f;
            break;
        default:
            spawnPosition.y += 1.0f;
            break;
    }

    ServerActor *rocket = owner.spawnActor(owner.getLevelFor(player), FIREWORK_ACTOR, spawnPosition);
    if (rocket == nullptr)
        return false;

    rocket->setProjectile(true);
    rocket->setMotion(Vector3f(gaussianDrift(), 0.05f, gaussianDrift()));
    rocket->getProjectileData().mFireworkLifetime = rollLifetime();
    rocket->getProjectileData().mFireworkData = buildFireworkData(item);
    owner.syncActorFirework(*rocket);

    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::LAUNCH, spawnPosition);
    player.consumeOneHeldItem();
    return true;
}
