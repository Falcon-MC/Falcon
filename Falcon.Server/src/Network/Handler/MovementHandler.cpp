#include "Network/Handler/MovementHandler.h"

#include "Actor/ActorFlags.h"
#include "Actor/RideSystem.h"
#include "Actor/ServerPlayer.h"
#include "Block/Block.h"
#include "Block/Systems/LavaResetFallDistanceSystem.h"
#include "Block/Systems/FireBlocksFetch.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Block/Systems/WaterMoveSystem.h"
#include "Level/Level.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Network/Handler/BadPacketHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Packets/SetActorMotionPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
    bool consumesAir(const ServerPlayer &player) {
        const int32_t respiration = ItemEnchantments::getLevel(
                player.getInventory().getArmor(PlayerInventory::ARMOR_HEAD), EnchantmentIds::RESPIRATION);
        return respiration <= 0 || std::rand() % (respiration + 1) == 0;
    }

    const float PLAYER_BASE_OFFSET = 1.62f;
    const float PLAYER_HALF_WIDTH = 0.3f;
    const float PLAYER_HEIGHT = 1.8f;
    const float GROUND_PROBE_DEPTH = 0.5f;
    const float AABB_EPSILON = 0.001f;
    const float SPRINT_EXHAUSTION_PER_BLOCK = 0.1f;
    const float SWIM_EXHAUSTION_PER_BLOCK = 0.01f;
    const float JUMP_EXHAUSTION = 0.05f;
    const float SPRINT_JUMP_EXHAUSTION = 0.2f;
    const char *TURTLE_HELMET = "minecraft:turtle_helmet";
    const int TURTLE_HELMET_TICKS = 200;
    const int DROWNING_AIR = -20;
    const float DROWNING_DAMAGE = 2.0f;
    const int AIR_REFILL_PER_TICK = 5;

    void ignitePlayer(ServerPlayer &player) {
        int fireProtectionLevel = 0;
        const PlayerInventory &inventory = player.getInventory();
        for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; ++slot)
            fireProtectionLevel = std::max(fireProtectionLevel,
                                           ItemEnchantments::getLevel(inventory.getArmor(slot),
                                                                      EnchantmentIds::FIRE_PROTECTION));

        const int reducedSeconds = (int) (8.0f * (float) fireProtectionLevel * 0.15f);
        const int fireTicks = std::max(1, 8 - std::min(8, reducedSeconds)) * 20;
        player.setFireTicks(std::max(player.getFireTicks(), fireTicks));
    }

    bool findSupportBlock(Level &level, const Vector3f &feetPosition, BlockState &support) {
        const int32_t lowestY = (int32_t) std::floor(feetPosition.y - GROUND_PROBE_DEPTH);
        const int32_t highestY = (int32_t) std::floor(feetPosition.y);
        const int32_t minX = (int32_t) std::floor(feetPosition.x - PLAYER_HALF_WIDTH + AABB_EPSILON);
        const int32_t maxX = (int32_t) std::floor(feetPosition.x + PLAYER_HALF_WIDTH - AABB_EPSILON);
        const int32_t minZ = (int32_t) std::floor(feetPosition.z - PLAYER_HALF_WIDTH + AABB_EPSILON);
        const int32_t maxZ = (int32_t) std::floor(feetPosition.z + PLAYER_HALF_WIDTH - AABB_EPSILON);

        for (int32_t y = highestY; y >= lowestY; --y) {
            BlockState firstSolid;
            bool foundSolid = false;

            for (int32_t x = minX; x <= maxX; ++x) {
                for (int32_t z = minZ; z <= maxZ; ++z) {
                    if (!level.isSolidAt(x, y, z))
                        continue;

                    const BlockState state = level.getBlockState(x, y, z);
                    if (!foundSolid)
                        firstSolid = state;
                    foundSolid = true;
                }
            }

            if (foundSolid) {
                support = firstSolid;
                return true;
            }
        }

        return false;
    }

    bool applyLandingBehavior(ServerNetworkHandler &owner, ServerPlayer &player, const Block &support,
                              float downwardVelocity) {
        if (!support.onEntityLand(player, downwardVelocity))
            return false;

        SetActorMotionPacket motion;
        motion.mRuntimeActorId = player.getRuntimeId();
        motion.mMotion = player.getMotion();
        motion.mTick = (uint64_t) owner.getCurrentTick();
        owner.getNetworkHandler().send(player.getNetworkIdentifier(), motion, owner.getCodecContext());
        return true;
    }
}

bool MovementHandler::checkGroundState(Level &level, const Vector3f &feetPosition) {
    const float shrink = 0.01f;
    const float minX = feetPosition.x - PLAYER_HALF_WIDTH + shrink;
    const float maxX = feetPosition.x + PLAYER_HALF_WIDTH - shrink;
    const float minZ = feetPosition.z - PLAYER_HALF_WIDTH + shrink;
    const float maxZ = feetPosition.z + PLAYER_HALF_WIDTH - shrink;

    const int32_t lowestY = (int32_t) std::floor(feetPosition.y - GROUND_PROBE_DEPTH);
    const int32_t highestY = (int32_t) std::floor(feetPosition.y);

    for (int32_t y = lowestY; y <= highestY; y++) {
        for (int32_t x = (int32_t) std::floor(minX); x <= (int32_t) std::floor(maxX); x++) {
            for (int32_t z = (int32_t) std::floor(minZ); z <= (int32_t) std::floor(maxZ); z++) {
                if (level.isSolidAt(x, y, z))
                    return true;
            }
        }
    }

    return false;
}

void MovementHandler::handleMovement(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &feetPosition,
                                     const Vector3f &rotation) {
    if (player.isRiding()) {
        player.setRotation(rotation);
        Actor *vehicle = RideSystem::resolve(owner, player.getVehicleId());
        if (vehicle != nullptr)
            RideSystem::syncPassengerPositions(owner, *vehicle);
        return;
    }

    const Vector3f previous = player.getPosition();
    player.setRotation(rotation);
    const float requestedX = feetPosition.x - previous.x;
    const float requestedY = feetPosition.y - previous.y;
    const float requestedZ = feetPosition.z - previous.z;

    player.setPosition(feetPosition);

    const bool wasOnGround = player.isOnGround();
    Level &level = owner.getLevelFor(player);
    const bool onGround = checkGroundState(level, feetPosition);
    player.setOnGround(onGround);

    const LiquidContact water = LiquidBlocksFetch::at(level, feetPosition);
    if (!player.wasRecentlyTeleported()) {
        const float distance = std::sqrt(requestedX * requestedX + requestedY * requestedY + requestedZ * requestedZ);
        const float horizontalDistance = std::sqrt(requestedX * requestedX + requestedZ * requestedZ);
        if (water.eyeInWater)
            player.exhaust(distance * SWIM_EXHAUSTION_PER_BLOCK);
        else if (water.feetInWater)
            player.exhaust(horizontalDistance * SWIM_EXHAUSTION_PER_BLOCK);
        else if (onGround && player.getFlags().get(ActorFlag::Sprinting))
            player.exhaust(horizontalDistance * SPRINT_EXHAUSTION_PER_BLOCK);
    }

    if (onGround) {
        BlockState support;
        const bool hasSupport = findSupportBlock(level, feetPosition, support);
        const Block supportBlock = hasSupport ? Block(support) : Block();

        if (!wasOnGround) {
            const bool landedOnSpecialBlock = hasSupport && applyLandingBehavior(owner, player, supportBlock,
                                                                                 std::min(0.0f, requestedY));
            if (!landedOnSpecialBlock)
                owner._handleFallDamage(player, hasSupport ? &supportBlock : nullptr);
        }

        player.resetFallDistance();
    } else {
        if (wasOnGround || requestedY > 0.0f || player.getMotion().y > 0.0f)
            player.resetFallDistance();

        if (feetPosition.y > player.getHighestPosition())
            player.setHighestPosition(feetPosition.y);

        player.updateFallDistance();
    }
}

void MovementHandler::tickFluidEffects(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.isSpawned() || player.isDead())
        return;

    const LiquidContact contact = LiquidBlocksFetch::at(owner.getLevelFor(player), player.getPosition());
    const FireContact fireContact = FireBlocksFetch::at(owner.getLevelFor(player), player.getPosition());
    WaterMoveSystem::tick(player, contact);
    LavaResetFallDistanceSystem::tick(player, contact);

    const bool creative = player.getGameType() == (int32_t) GameType::Creative;
    const bool fireResistance = player.hasEffect(MobEffectId::FireResistance);
    if (contact.lava || fireContact.fire)
        ignitePlayer(player);

    if (!creative && !fireResistance) {
        if (contact.lava)
            owner.applyDamage(player, 4.0f, "death.attack.lava", {player.getName()});
        if (fireContact.fire)
            owner.applyDamage(player, fireContact.damage, "death.attack.onFire", {player.getName()});
    }

    tickBreathing(owner, player, contact.eyeInWater);
}

void MovementHandler::tickBreathing(ServerNetworkHandler &owner, ServerPlayer &player, bool eyeInWater) {
    const int32_t gameType = player.getGameType();
    const bool invulnerable = gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator;

    bool breathing = !eyeInWater;
    const ItemStack &helmet = player.getInventory().getArmor(PlayerInventory::ARMOR_HEAD);
    const bool turtleHelmet = !helmet.isAir() && helmet.mDefinition != nullptr
                              && helmet.mDefinition->getIdentifier() == TURTLE_HELMET;

    if (breathing && turtleHelmet) {
        player.setTurtleHelmetTicks(TURTLE_HELMET_TICKS);
    } else if (player.getTurtleHelmetTicks() > 0) {
        breathing = true;
        player.setTurtleHelmetTicks(player.getTurtleHelmetTicks() - 1);
    }

    if (invulnerable)
        breathing = true;

    const bool breathingChanged = player.getFlags().get(ActorFlag::Breathing) != breathing;
    player.getFlags().set(ActorFlag::Breathing, breathing);

    const int previousAir = player.getAirSupply();
    const bool protectedFromWater = player.hasEffect(MobEffectId::WaterBreathing)
                                    || player.hasEffect(MobEffectId::ConduitPower);

    if (!protectedFromWater && eyeInWater) {
        if (invulnerable) {
            player.resetAirSupply();
        } else if ((player.getTurtleHelmetTicks() == 0 || player.getTurtleHelmetTicks() == TURTLE_HELMET_TICKS)
                   && consumesAir(player)) {
            int air = player.getAirSupply() - 1;
            if (air <= DROWNING_AIR) {
                air = 0;
                owner.applyDamage(player, DROWNING_DAMAGE, "death.attack.drown", {player.getName()});
            }
            player.setAirSupply(air);
        }
    } else if (player.getAirSupply() < ServerPlayer::MAX_AIR_SUPPLY) {
        player.setAirSupply(std::min(ServerPlayer::MAX_AIR_SUPPLY, player.getAirSupply() + AIR_REFILL_PER_TICK));
    }

    if (breathingChanged || player.getAirSupply() != previousAir)
        owner._sendEntityData(player);
}

void MovementHandler::handlePlayerAuthInput(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                            ServerPlayer &player, const PlayerAuthInputPacket &packet) {
    const Vector3f feetPosition(packet.mPosition.x, packet.mPosition.y - PLAYER_BASE_OFFSET,
                                packet.mPosition.z);
    player.queueMove(feetPosition, packet.mRotation);
    player.setMotion(packet.mDelta);

    if (player.isSpawned()) {
        const int32_t chunkX = (int32_t) std::floor(feetPosition.x) >> 4;
        const int32_t chunkZ = (int32_t) std::floor(feetPosition.z) >> 4;

        if (!player.hasChunkPosition() || chunkX != player.getLastChunkX() || chunkZ != player.getLastChunkZ())
            owner._sendChunks(player);
    }

    ActorFlags &flags = player.getFlags();
    const ActorFlags previous = flags;

    // Sprinting and sneaking are states the client reports every tick, the start and stop flags only
    // mark their edges. Reacting to the edges alone leaves a flag stuck whenever an edge is missed.
    bool sprinting = packet.hasInputFlag((int32_t) PlayerAuthInputData::Sprinting)
                     || packet.hasInputFlag((int32_t) PlayerAuthInputData::StartSprinting);

    if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StopSprinting))
        sprinting = false;

    if (!player.canSprint())
        sprinting = false;

    flags.set(ActorFlag::Sprinting, sprinting);

    bool sneaking = packet.hasInputFlag((int32_t) PlayerAuthInputData::Sneaking)
                    || packet.hasInputFlag((int32_t) PlayerAuthInputData::StartSneaking);

    if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StopSneaking))
        sneaking = false;

    flags.set(ActorFlag::Sneaking, sneaking);

    if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StartSwimming))
        flags.set(ActorFlag::Swimming, true);
    if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StopSwimming))
        flags.set(ActorFlag::Swimming, false);

    if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StartGliding))
        flags.set(ActorFlag::Gliding, true);
    if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StopGliding))
        flags.set(ActorFlag::Gliding, false);

    if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StartCrawling))
        flags.set(ActorFlag::Crawling, true);
    if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StopCrawling))
        flags.set(ActorFlag::Crawling, false);

    if (flags.getLowBits() != previous.getLowBits() || flags.getHighBits() != previous.getHighBits())
        owner._sendEntityData(player);

    if (packet.hasInputFlag((int32_t) PlayerAuthInputData::StartUsingItem)) {
        player.clearAwaitingConsumableRelease();
        owner._useHeldItem(player);
    }

    for (const PlayerBlockActionData &action: packet.mPlayerActions) {
        if (action.mAction == PlayerActionType::StartBreak
            || action.mAction == PlayerActionType::BlockContinueDestroy) {
            if (player.hasLastBlockAttacked() && player.getLastBlockAttacked() == action.mBlockPosition)
                continue;

            BlockActionHandler::startBreakingBlock(owner, player, action.mBlockPosition, action.mFace);
            player.setLastBlockAttacked(action.mBlockPosition);
        } else if (action.mAction == PlayerActionType::ContinueBreak) {
            if (player.isBreakingBlock() && player.getBreakingBlockPosition() == action.mBlockPosition)
                player.setBreakingFace(action.mFace);

            player.setLastBlockAttacked(action.mBlockPosition);
        } else if (action.mAction == PlayerActionType::AbortBreak ||
                  action.mAction == PlayerActionType::StopBreak) {
            BlockActionHandler::stopBreakingBlock(owner, player);
            player.clearLastBlockAttacked();
        } else if (action.mAction == PlayerActionType::BlockPredictDestroy ||
                  action.mAction == PlayerActionType::DimensionChangeRequestOrCreativeDestroyBlock) {
            BlockActionHandler::completeBreakingBlock(owner, player, action.mBlockPosition);
            player.clearLastBlockAttacked();
        } else if (action.mAction == PlayerActionType::Jump) {
            if (!LiquidBlocksFetch::at(owner.getLevelFor(player), player.getPosition()).eyeInWater)
                player.exhaust(player.getFlags().get(ActorFlag::Sprinting) ? SPRINT_JUMP_EXHAUSTION : JUMP_EXHAUSTION);
        } else if (action.mAction == PlayerActionType::StartUsingItem) {
            player.clearAwaitingConsumableRelease();
            owner._useHeldItem(player);
        }
    }

    if (packet.mHasItemUseTransaction) {
        if (packet.mItemUseTransaction.mActionType == 0)
            BlockActionHandler::placeBlock(owner, player, packet.mItemUseTransaction);
        else if (packet.mItemUseTransaction.mActionType == 1)
            owner._useHeldItem(player);
        else if (packet.mItemUseTransaction.mActionType == 2)
            BlockActionHandler::completeBreakingBlock(owner, player, packet.mItemUseTransaction.mBlockPosition);
    }

    (void) id;
}
