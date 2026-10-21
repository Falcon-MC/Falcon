#include "Network/ServerNetworkHandler.h"

#include "World/Actor/ActorFlags.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Systems/BlockContactSystem.h"
#include "World/Level/Block/Systems/FurnaceSystem.h"
#include "World/Item/ElytraItem.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/MovementHandler.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"

#include <chrono>

namespace {
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

void ServerNetworkHandler::_applyPendingPlayerMove(ServerPlayer &player) {
    if (player.hasPendingMove() && player.getPendingMovePosition() != player.getPosition()) {
        PluginEvent moveEvent;
        moveEvent.mType = FALCON_EVENT_PLAYER_MOVE;
        moveEvent.mCancellable = true;
        moveEvent.mPlayer = &player;
        moveEvent.mFrom = player.getPosition();
        moveEvent.mTo = player.getPendingMovePosition();
        PluginManager::getInstance().dispatch(moveEvent);

        if (moveEvent.mCancelled)
            player.teleport(*this, moveEvent.mFrom);
        else if (moveEvent.mToChanged)
            player.teleport(*this, moveEvent.mTo);
    }

    if (player.hasPendingMove()) {
        const int32_t gameType = player.getGameType();

        if (gameType == (int32_t) GameType::Survival || gameType == (int32_t) GameType::Adventure) {
            MovementHandler::handleMovement(*this, player, player.getPendingMovePosition(),
                                            player.getPendingMoveRotation());
        } else {
            player.setPosition(player.getPendingMovePosition());
            player.setRotation(player.getPendingMoveRotation());
            if (player.hasSimulatedGround())
                player.setOnGround(player.isSimulatedOnGround());
        }
        player.clearPendingMove();
    }
}

void ServerNetworkHandler::_tickPlayer(ServerPlayer &player) {
    if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent)
        return;

    ProfiledPlayerScope playerScope(mProfiler, player);

    const bool wasOnFire = player.isOnFire();
    player.tickGroundTracking();

    ServerPlayerMovementComponent &inputs = player.getMovementInputs();
    inputs.beginTick(mCurrentTick, mProperties.getPlayerTickThrottledMaxTickCredits(),
                     mProperties.getPlayerTickThrottledInputBatchSize());
    const double previousTickMs = mTickDurationSamples.empty() ? 0.0 : mTickDurationSamples.back();
    // Vehicles still travel in tickActors; consuming several rider inputs here would overwrite their controls.
    const size_t steps = player.isRiding() ? 1 : 1 + ServerPlayerMovementComponent::catchupSteps(previousTickMs);
    for (size_t step = 0; step < steps; ++step) {
        if (player.isDead() || player.isAwaitingDimensionAck()) {
            inputs.clearPending();
            break;
        }
        const auto packet = inputs.takeNext();
        if (packet == nullptr)
            break;
        MovementHandler::handlePlayerAuthInput(*this, player.getNetworkIdentifier(), player, *packet);
        // Collision state, position and plugin cancellation must settle before the next replay step.
        _applyPendingPlayerMove(player);
    }

    player.tickCombat(1);
    player.tickItemCooldowns(mCurrentTick);
    player.tickShield(*this);
    player.tickSpinAttack(*this);
    ElytraItem::tickGliding(*this, player);
    FurnaceSystem::tick(*this, player);

    _applyPendingPlayerMove(player);

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
        hurt(player, 1.0f, ActorDamageSource::environment("death.attack.inFire", player.getName()));

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
        if (!BlockActionHandler::canBreakBlock(*this, player, breakingPosition)) {
            BlockActionHandler::stopBreakingBlock(*this, player);
        } else {
            BlockActionHandler::continueBreakingBlock(*this, player);

            if (player.isBreakingBlock() && player.getBreakingFxTicker() % BREAKING_FX_INTERVAL == 0)
                BlockActionHandler::sendBreakingFx(*this, player);
        }
    }

    player.tickSpawnInvulnerability();

    const bool wasSprinting = player.getFlags().get(ActorFlag::Sprinting);
    const bool naturalRegeneration = getWorldFor(player).getOverworld().getGameRules()
            .getBool("naturalregeneration");
    const bool hungerChanged = player.isSpawned()
                               && player.tickHunger(1, (int) mProperties.getDifficulty(), naturalRegeneration);
    if (hungerChanged)
        _sendAttributes(player);
    if (player.isSpawned() && player.refreshVisibleEffects())
        _sendEntityData(player);

    if (player.consumeStarveDamage())
        hurt(player, 1.0f,
             ActorDamageSource::environment("death.attack.starve", player.getName()).withoutArmor().withoutCooldown());
    if (wasSprinting != player.getFlags().get(ActorFlag::Sprinting))
        _sendEntityData(player);

    if (!player.hasChunkPosition())
        return;

    mProfiler.beginSection(ProfilerSection::ChunkStreaming);
    _sendChunks(player);
    _checkTerrainReady(player);
    mProfiler.endSection(ProfilerSection::ChunkStreaming);
}
