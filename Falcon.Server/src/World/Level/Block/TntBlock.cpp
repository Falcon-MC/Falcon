#include "World/Level/Block/TntBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(TntBlock, 410);

#include "World/Actor/Misc/PrimedTntActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Core/Math/MathConstants.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Level/Level.h"
#include "World/Level/Block/DispenseSource.h"
#include "World/Level/Vibration/GameEvent.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Protocol/Types/ItemDefinition.h"

#include <cmath>
#include <random>

namespace {
    const float PRIME_HORIZONTAL_MOTION = 0.02f;
    const float PRIME_VERTICAL_MOTION = 0.2f;
    const char *ARROW = "minecraft:arrow";
    const char *SMALL_FIREBALL = "minecraft:small_fireball";

    // LevelEvent 1005, the fuse the dispenser sends
    const int32_t LEVEL_EVENT_FUSE = 1005;

    float nextAngle() {
        static std::mt19937 random(std::random_device{}());
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(random) * MathConstants::TWO_PI_F;
    }

    Vector3f primeMotion() {
        const float angle = nextAngle();
        return Vector3f(-std::sin(angle) * PRIME_HORIZONTAL_MOTION, PRIME_VERTICAL_MOTION,
                        -std::cos(angle) * PRIME_HORIZONTAL_MOTION);
    }
}

bool TntBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:tnt";
}

void TntBlock::prime(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int32_t fuse) {
    const BlockState air("minecraft:air");
    level.setBlockState(position.x, position.y, position.z, air);
    BlockActionHandler::broadcastBlockUpdate(owner, level, position,
                                             level.getBlockState(position.x, position.y, position.z));

    const Vector3f spawnPosition((float) position.x + 0.5f, (float) position.y, (float) position.z + 0.5f);
    owner.spawnPrimedTnt(level, spawnPosition, primeMotion(), fuse);
    owner.playNamedSound(level, PlaySoundName::FUSE, spawnPosition, 1.0f, 1.0f);
}

// TntBlock::dispense (sub_14B27A1D0, 26.60): refused while tntexplodes is off (sub_14B27A720 reads game rule
// 21, the rule every TNT ignition reads); the primed TNT spawns through the same call as a primed block
bool TntBlock::dispense(const DispenseSource &source) const {
    if (!source.mLevel.getGameRules().getBool("tntexplodes"))
        return false;

    const Vector3f spawnPosition(std::floor(source.mPosition.x) + 0.5f, std::floor(source.mPosition.y) + 0.5f,
                                 std::floor(source.mPosition.z) + 0.5f);
    if (source.mOwner.spawnPrimedTnt(source.mLevel, spawnPosition, primeMotion(), PrimedTntActor::DEFAULT_FUSE)
        == nullptr)
        return false;

    source.playClick();
    source.mOwner.broadcastLevelEvent(source.mLevel, LEVEL_EVENT_FUSE, source.mPosition, 0);
    source.mLevel.postGameEvent(GameEvents::ENTITY_PLACE, source.mPosition);
    source.removeOne();
    return true;
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
