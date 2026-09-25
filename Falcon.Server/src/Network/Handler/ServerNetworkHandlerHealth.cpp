#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/RideSystem.h"
#include "Actor/ServerPlayer.h"
#include "Block/Block.h"
#include "Block/BlockData.h"
#include "Block/BlockShape.h"
#include "Block/Blocks/BedBlock.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Event/GameEvents.h"
#include "Core/Math/MathConstants.h"
#include "Item/ItemData.h"
#include "Item/ItemEnchantments.h"
#include "Item/Items/TotemItem.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Plugin/PluginManager.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/DeathInfoPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Packets/RespawnPacket.h"
#include "Protocol/Packets/SetHealthPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace {
    const float PLAYER_BASE_OFFSET = 1.62f;
    const float BED_HEIGHT = 0.5625f;
    const float THROW_SPEED = 0.3f;
    const int THROW_PICKUP_DELAY = 40;
    const float ITEM_DROP_HEIGHT = 1.3f;
    const float EYE_HEIGHT_FACTOR = 0.9f;
    const float PLAYER_COLLISION_HEIGHT = 1.8f;
    const float SUFFOCATION_DAMAGE = 1.0f;

    bool isDamageDisabledByGameRule(const GameRules &rules, const std::string &deathMessageKey) {
        if (deathMessageKey.rfind("death.fell.", 0) == 0 || deathMessageKey == "death.attack.stalagmite")
            return !rules.getBool("falldamage");

        if (deathMessageKey == "death.attack.lava" || deathMessageKey == "death.attack.onFire"
            || deathMessageKey == "death.attack.inFire" || deathMessageKey == "death.attack.hotFloor")
            return !rules.getBool("firedamage");

        if (deathMessageKey == "death.attack.drown")
            return !rules.getBool("drowningdamage");

        if (deathMessageKey == "death.attack.freeze")
            return !rules.getBool("freezedamage");

        return false;
    }

    int protectionFactor(int level, float modifier) {
        if (level <= 0)
            return 0;
        return (int) std::floor((6.0f + (float) (level * level)) * modifier / 3.0f);
    }

    float applyArmorModifiers(const ServerPlayer &player, float amount, const std::string &deathMessageKey,
                              float efficiency) {
        const bool fireDamage = deathMessageKey == "death.attack.lava"
                                || deathMessageKey == "death.attack.onFire"
                                || deathMessageKey == "death.attack.inFire";
        const bool fallDamage = deathMessageKey == "death.fell.accident.generic";
        const bool explosionDamage = deathMessageKey == "death.attack.explosion";
        const bool projectileDamage = deathMessageKey == "death.attack.arrow";
        const bool armorDamage = deathMessageKey != "death.attack.inFire"
                                 && deathMessageKey != "death.attack.drown"
                                 && !fallDamage
                                 && deathMessageKey != "death.attack.outOfWorld"
                                 && deathMessageKey != "death.attack.thorns"
                                 && deathMessageKey != "death.attack.suicide";

        int armorPoints = 0;
        int enchantmentProtectionFactor = 0;
        for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; ++slot) {
            const ItemStack &armor = player.getInventory().getArmor(slot);
            if (armor.isAir() || armor.mDefinition == nullptr)
                continue;

            const ItemData *data = ItemDataTable::find(armor.mDefinition->getIdentifier());
            if (data != nullptr)
                armorPoints += data->mArmorPoints;

            enchantmentProtectionFactor += protectionFactor(
                    ItemEnchantments::getLevel(armor, EnchantmentIds::PROTECTION), 0.75f);
            if (fireDamage)
                enchantmentProtectionFactor += protectionFactor(
                        ItemEnchantments::getLevel(armor, EnchantmentIds::FIRE_PROTECTION), 1.25f);
            if (fallDamage)
                enchantmentProtectionFactor += protectionFactor(
                        ItemEnchantments::getLevel(armor, EnchantmentIds::FEATHER_FALLING), 2.5f);
            if (explosionDamage)
                enchantmentProtectionFactor += protectionFactor(
                        ItemEnchantments::getLevel(armor, EnchantmentIds::BLAST_PROTECTION), 1.5f);
            if (projectileDamage)
                enchantmentProtectionFactor += protectionFactor(
                        ItemEnchantments::getLevel(armor, EnchantmentIds::PROJECTILE_PROTECTION), 1.5f);
        }

        const float effectivePoints = (float) armorPoints * efficiency;
        const float effectiveFactor = (float) enchantmentProtectionFactor * efficiency;

        if (armorDamage)
            amount *= std::max(0.0f, 1.0f - std::min(1.0f, effectivePoints * 0.04f));

        if (effectiveFactor > 0.0f) {
            const int scaledProtection = std::min(
                    (int) std::ceil(std::min(effectiveFactor, 25.0f) *
                                    (50.0f + (float) (rand() % 51)) / 100.0f), 20);
            amount *= std::max(0.0f, 1.0f - (float) scaledProtection * 0.04f);
        }

        return amount;
    }
}

void ServerNetworkHandler::_sendHealth(ServerPlayer &player) {
    SetHealthPacket health;
    health.mHealth = (int32_t) std::ceil(player.getHealth());

    mNetworkHandler->send(player.getNetworkIdentifier(), health, mCodecContext);

    _sendAttributes(player);
}

void ServerNetworkHandler::_handleFallDamage(ServerPlayer &player, const Block *supportBlock) {
    if (player.isFlying() || player.isDead())
        return;

    float damage = player.computeFallDamage();
    if (supportBlock != nullptr) {
        const std::optional<float> blockDamage = supportBlock->getFallDamage(player, damage);
        if (blockDamage.has_value())
            damage = std::max(0.0f, *blockDamage);
    }

    if (damage < 1.0f)
        return;

    hurt(player, damage, DamageSource::environment("death.fell.accident.generic", player.getName()));
}

void ServerNetworkHandler::_handleVoidDamage(ServerPlayer &player) {
    if (!player.isSpawned() || player.isDead())
        return;

    if (player.getPosition().y > (float) (LevelChunk::MIN_Y - 16))
        return;

    hurt(player, 10.0f, DamageSource::environment("death.attack.outOfWorld", player.getName()));
}

bool ServerNetworkHandler::_isEyeInsideSolidBlock(Level &level, const Vector3f &position, float height) {
    const float eyeY = position.y + height * EYE_HEIGHT_FACTOR;
    const int32_t blockX = (int32_t) std::floor(position.x);
    const int32_t blockY = (int32_t) std::floor(eyeY);
    const int32_t blockZ = (int32_t) std::floor(position.z);

    if (blockY < level.getMinY() || blockY > level.getMaxY())
        return false;

    const BlockState *state = level.peekBlockPtr(blockX, blockY, blockZ);
    if (state == nullptr)
        return false;

    const BlockData *data = BlockDataTable::find(state->mName.c_str());
    if (data == nullptr || !data->mSolid || data->mTransparent)
        return false;

    return BlockShape::isPositionInside(*state, blockX, blockY, blockZ, position.x, eyeY, position.z);
}

void ServerNetworkHandler::_handleSuffocationDamage(ServerPlayer &player) {
    if (!player.isSpawned() || player.isDead())
        return;

    const int32_t gameType = player.getGameType();
    if (gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator)
        return;

    if (!_isEyeInsideSolidBlock(getLevelFor(player), player.getPosition(), PLAYER_COLLISION_HEIGHT))
        return;

    hurt(player, SUFFOCATION_DAMAGE, DamageSource::environment("death.attack.inWall", player.getName()));
}

DamageResult ServerNetworkHandler::hurt(ServerPlayer &player, float amount, const DamageSource &source) {
    const std::string &key = source.mDeathMessageKey;
    if (!player.isSpawned() || player.isDead() || amount <= 0.0f)
        return DamageResult::Ignored;

    const int32_t gameType = player.getGameType();
    if (gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator)
        return DamageResult::Ignored;

    if (player.isSpawnInvulnerable() && key != "death.attack.suicide")
        return DamageResult::Ignored;

    if (isDamageDisabledByGameRule(getLevelFor(player).getGameRules(), key))
        return DamageResult::Ignored;

    if (mScriptEngine.beforeEntityHurt(player, amount, key, source.mAttacker))
        return DamageResult::Ignored;

    const bool cooling = source.mRespectCooldown && key != "death.attack.suicide" && player.getNoDamageTicks() > 0;
    if (cooling && player.getLastDamageAmount() >= amount)
        return DamageResult::Ignored;

    PluginEvent damageEvent;
    damageEvent.mType = FALCON_EVENT_ENTITY_DAMAGE;
    damageEvent.mCancellable = true;
    damageEvent.mEntity = &player;
    damageEvent.mAttacker = source.mAttacker;
    damageEvent.mAmount = amount;
    damageEvent.mCause = key;
    PluginManager::getInstance().dispatch(damageEvent);
    if (damageEvent.mCancelled)
        return DamageResult::Ignored;

    amount = (float) damageEvent.mAmount;
    if (amount <= 0.0f)
        return DamageResult::Ignored;

    if (cooling && player.getLastDamageAmount() >= amount)
        return DamageResult::Ignored;

    if (source.mOrigin.has_value()) {
        Actor *knockedBack = source.mProjectile ? nullptr : source.mAttacker;
        if (player.blockWithShield(*this, *source.mOrigin, amount, knockedBack, source.mDisablesShield)) {
            if (source.mRespectCooldown) {
                player.setNoDamageTicks(10);
                player.setLastDamageAmount(amount);
            }
            return DamageResult::Blocked;
        }
    }

    const float rawAmount = amount;
    if (cooling)
        amount -= player.getLastDamageAmount();

    if (source.mRespectCooldown) {
        player.setNoDamageTicks(10);
        player.setLastDamageAmount(rawAmount);
    }

    if (source.mApplyArmor)
        amount = applyArmorModifiers(player, amount, key, source.mArmorEfficiency);

    if (key != "death.attack.outOfWorld" && key != "death.attack.suicide") {
        if (const MobEffectInstance *resistance = player.getEffect(MobEffectId::Resistance))
            amount *= 1.0f - std::min(1.0f, 0.2f * (float) resistance->level());
    }

    if (amount <= 0.0f)
        return DamageResult::Ignored;

    if (player.getHealth() - amount < 1.0f && key != "death.attack.outOfWorld" &&
        key != "death.attack.suicide" && TotemItem::consume(*this, player))
        return DamageResult::Dealt;

    const float health = player.reduceHealth(amount);

    EntityHurtAfterEvent hurtEvent(player, amount, key);
    mEventBus.after().mEntityHurt.emit(hurtEvent);

    if (health <= 0.0f) {
        killPlayer(player, key, source.mDeathMessageParameters);
        return DamageResult::Dealt;
    }

    if (source.mAttacker != nullptr && player.catchFireFrom(*source.mAttacker, mProperties.getDifficulty()))
        _sendEntityData(player);

    _sendHealth(player);
    _broadcastEntityEvent(player, (uint8_t) EntityEventType::HurtAnimation);
    return DamageResult::Dealt;
}

void ServerNetworkHandler::killPlayer(ServerPlayer &player, const std::string &deathMessageKey,
                                      const std::vector<std::string> &deathMessageParameters) {
    if (player.isDead())
        return;

    stopSleep(player);
    if (player.isRiding())
        RideSystem::dismount(*this, player, false);
    RideSystem::ejectAll(*this, player);
    player.kill();
    player.setOnFire(false);
    _sendEntityData(player);

    EntityDieAfterEvent dieEvent(player, deathMessageKey);
    mEventBus.after().mEntityDie.emit(dieEvent);

    player.getInventoryManager().onCurrentWindowRemove();

    const GameRules &rules = getLevelFor(player).getGameRules();
    const bool keepInventory = rules.getBool("keepinventory");

    if (!keepInventory) {
        _dropInventoryOnDeath(player);

        if (player.getGameType() != (int32_t) GameType::Creative) {
            const int droppedExperience = player.getExperience().getXpDropAmount();
            if (droppedExperience > 0)
                spawnExperienceOrbs(getLevelFor(player), player.getPosition(), droppedExperience);
        }

        player.getExperience().reset();
        player.syncExperience();
        _sendAttributes(player);
    }

    _sendHealth(player);
    _broadcastEntityEvent(player, (uint8_t) EntityEventType::DeathAnimation);

    const std::string key = deathMessageKey.empty() ? "death.attack.generic" : deathMessageKey;
    const std::vector<std::string> parameters = deathMessageParameters.empty()
                                                ? std::vector<std::string>{player.getName()}
                                                : deathMessageParameters;

    std::string deathMessage = key;
    PluginEvent deathEvent;
    deathEvent.mType = FALCON_EVENT_PLAYER_DEATH;
    deathEvent.mPlayer = &player;
    deathEvent.mEntity = &player;
    deathEvent.mMessage = &deathMessage;
    PluginManager::getInstance().dispatch(deathEvent);

    if (rules.getBool("showdeathmessages")) {
        if (deathMessage == key)
            broadcastTranslation(key, parameters);
        else if (!deathMessage.empty())
            broadcastSystemMessage(deathMessage);
    }

    const Vector3f spawn = mLevel.getSpawnPositionForPlayer();

    RespawnPacket respawn;
    respawn.mPosition = Vector3f(spawn.x, spawn.y + PLAYER_BASE_OFFSET, spawn.z);
    respawn.mState = RespawnPacket::State::ServerSearching;
    respawn.mRuntimeActorId = player.getRuntimeId();
    mNetworkHandler->send(player.getNetworkIdentifier(), respawn, mCodecContext);

    DeathInfoPacket info;
    info.mCauseAttackName = key;
    info.mMessageList = parameters;
    mNetworkHandler->send(player.getNetworkIdentifier(), info, mCodecContext);

    LOG_INFO(LogAreaID::Server, "%s died", player.getName().c_str());
}

void ServerNetworkHandler::_throwItem(ServerPlayer &player, const ItemStack &item) {
    if (item.isAir() || item.mCount <= 0)
        return;

    const Vector3f position = player.getPosition();
    const Vector3f dropPosition(position.x, position.y + ITEM_DROP_HEIGHT, position.z);

    const float yaw = player.getRotation().y * MathConstants::PI_F / 180.0f;
    const float pitch = player.getRotation().x * MathConstants::PI_F / 180.0f;

    const Vector3f motion(-std::sin(yaw) * std::cos(pitch) * THROW_SPEED,
                          -std::sin(pitch) * THROW_SPEED + 0.1f,
                          std::cos(yaw) * std::cos(pitch) * THROW_SPEED);

    dropItem(getLevelFor(player), dropPosition, item, motion, THROW_PICKUP_DELAY);
    mScriptEngine.onEntityItemDrop(player, item);
}

void ServerNetworkHandler::_dropInventoryOnDeath(ServerPlayer &player) {
    PlayerInventory &inventory = player.getInventory();
    Level &level = getLevelFor(player);
    const Vector3f position = player.getPosition();
    const Vector3f dropPosition(position.x, position.y + ITEM_DROP_HEIGHT, position.z);

    const auto dropOnDeath = [&](const ItemStack &item) {
        if (ItemEnchantments::getLevel(item, EnchantmentIds::VANISHING) > 0)
            return;

        dropItem(level, dropPosition, item, ItemActorHandler::randomDropAroundMotion(),
                 ItemActorHandler::DEATH_DROP_PICKUP_DELAY);
    };

    for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; slot++)
        dropOnDeath(inventory.getItem(slot));

    for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; slot++)
        dropOnDeath(inventory.getArmor(slot));

    dropOnDeath(inventory.getOffhand());
    dropOnDeath(inventory.getCursor());

    for (int slot = 0; slot < PlayerInventory::CRAFTING_SIZE; ++slot)
        dropOnDeath(inventory.getCraftingItem(slot));

    for (int slot = 0; slot < PlayerInventory::CRAFTING_TABLE_SIZE; ++slot)
        dropOnDeath(inventory.getCraftingTableItem(slot));

    for (int slot = 0; slot < PlayerInventory::FURNACE_SIZE; ++slot)
        dropOnDeath(inventory.getFurnaceItem(slot));

    inventory.clear();
    inventory.setSelectedSlot(0);

    _sendInventory(player);
}

Vector3f ServerNetworkHandler::_respawnPositionFor(ServerPlayer &player) {
    if (!player.hasSpawnPoint())
        return mLevel.getSpawnPositionForPlayer();

    const Vector3i bed = player.getSpawnPoint();
    if (!BedBlock::isValidAt(mLevel, bed)) {
        player.clearSpawnPoint();
        player.sendTranslation("§7%tile.bed.notValid", {});
        return mLevel.getSpawnPositionForPlayer();
    }

    return Vector3f((float) bed.x + 0.5f, (float) bed.y + BED_HEIGHT, (float) bed.z + 0.5f);
}

void ServerNetworkHandler::_respawnPlayer(ServerPlayer &player) {
    if (!player.isDead())
        return;

    Vector3f spawn = _respawnPositionFor(player);

    PluginEvent respawnEvent;
    respawnEvent.mType = FALCON_EVENT_PLAYER_RESPAWN;
    respawnEvent.mPlayer = &player;
    respawnEvent.mTo = spawn;
    PluginManager::getInstance().dispatch(respawnEvent);
    if (respawnEvent.mToChanged)
        spawn = respawnEvent.mTo;

    if (player.getDimension() != DimensionType::Overworld)
        changePlayerDimension(player, DimensionType::Overworld, spawn);

    player.setDead(false);
    player.grantSpawnInvulnerability();
    player.setHealth(player.getMaxHealth());
    player.teleport(spawn);
    player.markTeleported();
    player.setRotation(Vector3f(0.0f, 0.0f, 0.0f));
    player.clearPendingMove();
    player.resetAirSupply();
    player.extinguish();

    const Vector3f eyePosition(spawn.x, spawn.y + PLAYER_BASE_OFFSET, spawn.z);

    RespawnPacket respawn;
    respawn.mPosition = eyePosition;
    respawn.mState = RespawnPacket::State::ServerReady;
    respawn.mRuntimeActorId = player.getRuntimeId();
    mNetworkHandler->send(player.getNetworkIdentifier(), respawn, mCodecContext);

    MovePlayerPacket move;
    move.mRuntimeActorId = (int64_t) player.getRuntimeId();
    move.mPosition = eyePosition;
    move.mRotation = player.getRotation();
    move.mMode = MovePlayerMode::Respawn;
    move.mOnGround = true;
    mNetworkHandler->send(player.getNetworkIdentifier(), move, mCodecContext);

    _sendHealth(player);
    _sendEntityData(player);
    _sendAbilities(player);
    _sendInventory(player);
    ItemActorHandler::sendItemActorsTo(*this, player);
    sendActorsTo(player);
    _sendChunks(player);

    LOG_INFO(LogAreaID::Server, "%s respawned at %.2f %.2f %.2f", player.getName().c_str(), spawn.x, spawn.y,
             spawn.z);
}
