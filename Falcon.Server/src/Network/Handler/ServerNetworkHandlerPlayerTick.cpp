#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ActorFlags.h"
#include "Actor/ServerPlayer.h"
#include "Block/Systems/BlockContactSystem.h"
#include "Block/Systems/FurnaceSystem.h"
#include "Item/Items/ElytraItem.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/MovementHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <chrono>

namespace {
    const float MAX_BREAKING_DISTANCE_SQUARED = 256.0f;
    const int32_t BREAKING_FX_INTERVAL = 5;

    class ProfiledPlayerScope {
    public:
        ProfiledPlayerScope(Profiler &profiler, const ServerPlayer &player)
                : mProfiler(profiler), mPlayer(player), mChunksBefore(player.getSentChunkCount()),
                  mStart(std::chrono::steady_clock::now()) {
        }

        ~ProfiledPlayerScope() {
            if (!mProfiler.isActive())
                return;

            const double elapsed = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - mStart).count();

            mProfiler.recordPlayer(mPlayer.getName(), elapsed,
                                   (uint32_t) (mPlayer.getSentChunkCount() - mChunksBefore));
        }

    private:
        Profiler &mProfiler;
        const ServerPlayer &mPlayer;
        size_t mChunksBefore;
        std::chrono::steady_clock::time_point mStart;
    };
}

void ServerNetworkHandler::_tickPlayer(ServerPlayer &player) {
    if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent)
        return;

    ProfiledPlayerScope playerScope(mProfiler, player);

    const bool wasOnFire = player.isOnFire();
    player.tickGroundTracking();
    player.tickCombat(1);
    player.tickItemCooldowns(mCurrentTick);
    player.tickShield(*this);
    player.tickSpinAttack(*this);
    ElytraItem::tickGliding(*this, player);
    FurnaceSystem::tick(*this, player);

    if (player.hasPendingMove()) {
        const int32_t gameType = player.getGameType();

        if (gameType == (int32_t) GameType::Survival || gameType == (int32_t) GameType::Adventure) {
            MovementHandler::handleMovement(*this, player, player.getPendingMovePosition(),
                                            player.getPendingMoveRotation());
        } else {
            player.setPosition(player.getPendingMovePosition());
            player.setRotation(player.getPendingMoveRotation());
        }
        player.clearPendingMove();
    }

    _handleVoidDamage(player);
    _handleSuffocationDamage(player);
    MovementHandler::tickFluidEffects(*this, player);
    BlockContactSystem::tick(*this, player);

    const bool fireTickDamage = !player.isDead() && player.tickFire();

    if (player.getGameType() == (int32_t) GameType::Creative && player.getFireTicks() > 1)
        player.setFireTicks(1);

    if (fireTickDamage && player.getGameType() != (int32_t) GameType::Creative &&
        player.getGameType() != (int32_t) GameType::Spectator &&
        !player.hasEffect(MobEffectId::FireResistance))
        hurt(player, 1.0f, DamageSource::environment("death.attack.inFire", player.getName()));

    if (wasOnFire != player.isOnFire())
        _sendEntityData(player);

    _tickItemUse(player);

    const bool effectAttributesDirty = player.getEffects().consumeAttributesDirty();
    const bool effectStateChanged = player.isSpawned() && player.tickEffects(1);
    if (player.isSpawned() && (effectAttributesDirty || effectStateChanged)) {
        _sendAttributes(player);
        _sendEntityData(player);
    }

    if (player.isBreakingBlock()) {
        const Vector3i breakingPosition = player.getBreakingBlockPosition();
        const Vector3f breakingCenter((float) breakingPosition.x + 0.5f, (float) breakingPosition.y + 0.5f,
                                      (float) breakingPosition.z + 0.5f);
        const Vector3f feet = player.getPosition();
        const float dx = feet.x - breakingCenter.x;
        const float dy = feet.y - breakingCenter.y;
        const float dz = feet.z - breakingCenter.z;

        if (dx * dx + dy * dy + dz * dz > MAX_BREAKING_DISTANCE_SQUARED) {
            BlockActionHandler::stopBreakingBlock(*this, player);
        } else {
            BlockActionHandler::continueBreakingBlock(*this, player);

            if (player.isBreakingBlock() && player.getBreakingFxTicker() % BREAKING_FX_INTERVAL == 0)
                BlockActionHandler::sendBreakingFx(*this, player);
        }
    }

    player.tickSpawnInvulnerability();

    const bool wasSprinting = player.getFlags().get(ActorFlag::Sprinting);
    const bool naturalRegeneration = getLevelFor(player).getGameRules().getBool("naturalregeneration");
    const bool hungerChanged = player.isSpawned()
                               && player.tickHunger(1, (int) mProperties.getDifficulty(), naturalRegeneration);
    if (hungerChanged)
        _sendAttributes(player);
    if (player.isSpawned() && player.refreshVisibleEffects())
        _sendEntityData(player);

    if (player.consumeStarveDamage())
        hurt(player, 1.0f,
             DamageSource::environment("death.attack.starve", player.getName()).withoutArmor().withoutCooldown());
    if (wasSprinting != player.getFlags().get(ActorFlag::Sprinting))
        _sendEntityData(player);

    if (!player.hasChunkPosition())
        return;

    mProfiler.beginSection(ProfilerSection::ChunkStreaming);
    _sendChunks(player);
    _checkTerrainReady(player);
    mProfiler.endSection(ProfilerSection::ChunkStreaming);
}
