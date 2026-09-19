#include "Block/Blocks/TntBlock.h"

#include "Actor/PrimedTntActor.h"
#include "Actor/ServerPlayer.h"
#include "Core/Math/MathConstants.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"

#include <cmath>
#include <random>

namespace {
    const float PRIME_HORIZONTAL_MOTION = 0.02f;
    const float PRIME_VERTICAL_MOTION = 0.2f;
    const char *FUSE_SOUND = "random.fuse";
    const char *ARROW = "minecraft:arrow";
    const char *SMALL_FIREBALL = "minecraft:small_fireball";

    float nextAngle() {
        static std::mt19937 random(std::random_device{}());
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(random) * MathConstants::TWO_PI_F;
    }
}

bool TntBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:tnt";
}

void TntBlock::prime(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int32_t fuse) {
    const BlockState air("minecraft:air");
    level.setBlockState(position.x, position.y, position.z, air);
    BlockActionHandler::broadcastBlockUpdate(owner, position, level.getBlockState(position.x, position.y, position.z));

    const Vector3f spawnPosition((float) position.x + 0.5f, (float) position.y, (float) position.z + 0.5f);
    const float angle = nextAngle();
    const Vector3f motion(-std::sin(angle) * PRIME_HORIZONTAL_MOTION, PRIME_VERTICAL_MOTION,
                          -std::cos(angle) * PRIME_HORIZONTAL_MOTION);

    owner.spawnPrimedTnt(spawnPosition, motion, fuse);
    owner.playNamedSound(FUSE_SOUND, spawnPosition, 1.0f, 1.0f);
}

bool TntBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                          const BlockState &state) const {
    (void) state;

    const ItemStack &held = player.getInventory().getItemInHand();
    if (held.isAir() || held.mDefinition == nullptr)
        return false;

    const std::string &identifier = held.mDefinition->getIdentifier();
    Level &level = owner.getLevelFor(player);

    if (identifier == "minecraft:flint_and_steel") {
        owner.damagePlayerHeldItem(player, 1);
        prime(owner, level, position, PrimedTntActor::DEFAULT_FUSE);
        return true;
    }

    if (identifier == "minecraft:fire_charge") {
        player.consumeOneHeldItem();
        prime(owner, level, position, PrimedTntActor::DEFAULT_FUSE);
        return true;
    }

    if (ItemEnchantments::getLevel(held, EnchantmentIds::FIRE_ASPECT) > 0) {
        owner.damagePlayerHeldItem(player, 1);
        prime(owner, level, position, PrimedTntActor::DEFAULT_FUSE);
        return true;
    }

    return false;
}

bool TntBlock::onProjectileHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state, ServerActor &projectile) const {
    (void) state;

    const std::string identifier = projectile.getIdentifier();
    const bool burningArrow = identifier == ARROW
                              && (projectile.isOnFire() || projectile.getProjectileData().mFlameTicks > 0);
    if (identifier != SMALL_FIREBALL && !burningArrow)
        return false;

    prime(owner, level, position, PrimedTntActor::DEFAULT_FUSE);
    return true;
}
