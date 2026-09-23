#include "Actor/ServerPlayer.h"

#include "Actor/DynamicPropertyStore.h"
#include "Actor/ActorClassRegistry.h"
#include "Actor/ServerActor.h"
#include "Block/Inventory/EnderChestInventoryStore.h"
#include "Inventory/ItemStackNbt.h"
#include "Inventory/InventoryManager.h"
#include "Item/ItemData.h"
#include "Item/ItemEnchantments.h"
#include "Item/VanillaItems.h"
#include "Level/Level.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/AnimatePacket.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/PlayerStartItemCooldownPacket.h"
#include "Protocol/Packets/SetActorMotionPacket.h"
#include "Protocol/Packets/SetTitlePacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "Protocol/Packets/MobEffectPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Server/Localization.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace {
    const char *TAG_POS = "Pos";
    const char *TAG_MOTION = "Motion";
    const char *TAG_ROTATION = "Rotation";
    const char *TAG_HEALTH = "Health";
    const char *TAG_FIRE = "Fire";
    const char *TAG_AIR = "Air";
    const char *TAG_LEVEL = "Level";
    const char *TAG_GAME_MODE = "playerGameType";
    const char *TAG_FIRST_PLAYED = "firstPlayed";
    const char *TAG_LAST_PLAYED = "lastPlayed";
    const char *TAG_LAST_KNOWN_XUID = "LastKnownXUID";
    const char *TAG_NAME = "NameTag";
    const char *TAG_INVENTORY = "Inventory";
    const char *TAG_OFF_INVENTORY = "OffInventory";
    const char *TAG_ENDER_ITEMS = "EnderItems";
    const char *TAG_SELECTED_SLOT = "SelectedInventorySlot";
    const char *TAG_FOOD_LEVEL = "foodLevel";
    const char *TAG_FOOD_EXHAUSTION_LEVEL = "foodExhaustionLevel";
    const char *TAG_FOOD_SATURATION_LEVEL = "foodSaturationLevel";
    const char *TAG_FOOD_TICK_TIMER = "foodTickTimer";
    const char *TAG_XP_LEVEL = "XpLevel";
    const char *TAG_XP_PROGRESS = "XpP";
    const char *TAG_LIFETIME_XP_TOTAL = "XpTotal";
    const char *TAG_ACTIVE_EFFECTS = "ActiveEffects";
    const char *TAG_SPAWN_X = "SpawnX";
    const char *TAG_SPAWN_Y = "SpawnY";
    const char *TAG_SPAWN_Z = "SpawnZ";

    int64_t currentTimeMillis() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    Tag floatList(float first, float second, float third) {
        return Tag::ofList(Tag::Type::Float, {Tag::ofFloat(first), Tag::ofFloat(second), Tag::ofFloat(third)});
    }

    float listValue(const Tag &data, const char *key, size_t index, float fallback) {
        const Tag *list = data.get(key);
        if (list == nullptr || list->getType() != Tag::Type::List)
            return fallback;

        const std::vector<Tag> &values = list->getList();
        if (index >= values.size())
            return fallback;

        return values[index].asFloat();
    }

    constexpr float MAX_REACH = 8.0f;
    constexpr float SPIN_ATTACK_DAMAGE = 8.0f;
    constexpr float SPIN_ATTACK_REACH = 1.0f;
    constexpr float PLAYER_WIDTH = 0.6f;
    constexpr float PLAYER_HEIGHT = 1.8f;
    constexpr int ATTACK_COOLDOWN_TICKS = 10;

    int protectionFactor(int level) {
        if (level <= 0)
            return 0;
        return (int) std::floor((6.0f + (float) (level * level)) * 0.75f / 3.0f);
    }

    void broadcastEvent(ServerNetworkHandler &owner, const Actor &actor, EntityEventType eventType) {
        ActorEventPacket packet;
        packet.mRuntimeActorId = actor.getRuntimeId();
        packet.mEventId = (uint8_t) eventType;
        packet.mEventData = 0;
        packet.mHasFirePosition = false;

        for (const auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned())
                owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), packet, owner.getCodecContext());
        }
    }

    void broadcastAnimation(ServerNetworkHandler &owner, const Actor &actor, AnimatePacket::Action action) {
        AnimatePacket packet;
        packet.mAction = action;
        packet.mRuntimeActorId = actor.getRuntimeId();
        packet.mData = 0.0f;
        packet.mSwingSource.clear();

        for (const auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned())
                owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), packet, owner.getCodecContext());
        }
    }

    void sendMotion(ServerNetworkHandler &owner, const ServerPlayer &player) {
        SetActorMotionPacket packet;
        packet.mRuntimeActorId = player.getRuntimeId();
        packet.mMotion = player.getMotion();
        packet.mTick = (uint64_t) owner.getCurrentTick();

        for (const auto &entry: owner.getPlayers()) {
            if (entry.second.isSpawned())
                owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), packet, owner.getCodecContext());
        }
    }

    bool shouldDamageDurability(const ItemStack &item) {
        const int unbreaking = ItemEnchantments::getLevel(item, EnchantmentIds::UNBREAKING);
        return unbreaking <= 0 || std::rand() % (unbreaking + 1) == 0;
    }

    bool damageItem(ItemStack &item, int amount) {
        if (item.isAir() || item.mDefinition == nullptr || amount <= 0)
            return false;

        const ItemData *data = ItemDataTable::find(item.mDefinition->getIdentifier());
        if (data == nullptr || data->mMaxDurability <= 0)
            return false;

        int applied = 0;
        for (int index = 0; index < amount; ++index) {
            if (shouldDamageDurability(item))
                ++applied;
        }
        item.mDamage += applied;
        if (item.mDamage >= data->mMaxDurability)
            item = ItemStack::air();
        return applied > 0;
    }

    void damageArmor(ServerNetworkHandler &owner, ServerPlayer &victim, float baseDamage) {
        const int durability = std::max(1, (int) std::floor(baseDamage / 4.0f));
        bool changed = false;
        PlayerInventory &inventory = victim.getInventory();
        for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; ++slot) {
            ItemStack armor = inventory.getArmor(slot);
            if (armor.isAir() || !damageItem(armor, durability))
                continue;
            inventory.setArmor(slot, std::move(armor));
            changed = true;
        }

        if (changed) {
            victim.getInventoryManager().syncContents(InventoryManager::InventoryId::Armor);
            InventoryHandler::sendArmorContent(owner, victim);
        }
    }

    float armorReducedDamage(const ServerPlayer &victim, float amount, float armorEfficiency = 1.0f) {
        float armorPoints = 0.0f;
        float epf = 0.0f;
        for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; ++slot) {
            const ItemStack &armor = victim.getInventory().getArmor(slot);
            if (armor.isAir() || armor.mDefinition == nullptr)
                continue;

            const ItemData *data = ItemDataTable::find(armor.mDefinition->getIdentifier());
            if (data != nullptr)
                armorPoints += (float) data->mArmorPoints;
            epf += (float) protectionFactor(ItemEnchantments::getLevel(armor, EnchantmentIds::PROTECTION));
        }

        armorPoints *= armorEfficiency;
        epf *= armorEfficiency;

        amount *= std::max(0.0f, 1.0f - std::min(1.0f, armorPoints * 0.04f));
        if (epf > 0.0f) {
            const int scaled = std::min((int) std::ceil(std::min(epf, 25.0f) *
                                                        (50.0f + (float) (std::rand() % 51)) / 100.0f), 20);
            amount *= std::max(0.0f, 1.0f - (float) scaled * 0.04f);
        }
        return amount;
    }

    void damageHeldItem(ServerPlayer &attacker) {
        if (attacker.getGameType() == (int32_t) GameType::Creative)
            return;

        PlayerInventory &inventory = attacker.getInventory();
        const int slot = inventory.getSelectedSlot();
        ItemStack held = inventory.getItemInHand();
        if (!damageItem(held, 1))
            return;

        inventory.setItem(slot, std::move(held));
        attacker.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
    }
}

ServerPlayer::ServerPlayer(const NetworkIdentifier &id, uint64_t runtimeId, PacketSender *sender)
        : Actor(runtimeId), mId(id), mLoginState(LoginState::Connecting), mSender(sender) {
    getEffects().setEventCallback([this](const MobEffectInstance &effect, MobEffectEvent event) {
        if (mSender == nullptr || !mEffectsNetworkReady)
            return;

        MobEffectPacket packet;
        packet.mRuntimeActorId = getRuntimeId();
        packet.mEvent = event == MobEffectEvent::Add ? MobEffectPacket::Event::Add
                         : event == MobEffectEvent::Modify ? MobEffectPacket::Event::Modify
                         : MobEffectPacket::Event::Remove;
        packet.mEffectId = (int32_t) effect.mId;
        packet.mAmplifier = event == MobEffectEvent::Remove ? 0 : effect.mAmplifier;
        packet.mParticles = event == MobEffectEvent::Remove ? false : effect.mParticles;
        packet.mDuration = event == MobEffectEvent::Remove ? 0 : effect.mDuration;
        packet.mTick = 0;
        packet.mAmbient = event == MobEffectEvent::Remove ? false : effect.mAmbient;
        mSender->sendPacketTo(mId, packet);
    });
}

std::string ServerPlayer::localize(const std::string &key, const std::vector<std::string> &parameters) const {
    return Localization::getInstance().translate(mLocale, key, parameters);
}

float ServerPlayer::_applyAttackerModifiers(float baseDamage, float damage) const {
    if (const MobEffectInstance *strength = getEffect(MobEffectId::Strength))
        damage += baseDamage * 0.3f * (float) strength->level();
    if (const MobEffectInstance *weakness = getEffect(MobEffectId::Weakness))
        damage -= baseDamage * 0.2f * (float) weakness->level();
    damage = std::max(0.0f, damage);

    if (_isCriticalHit())
        damage += damage * 0.5f;

    return damage;
}

bool ServerPlayer::_isCriticalHit() const {
    return !isFlying() && getFallDistance() > 0.0f && !hasEffect(MobEffectId::Blindness)
           && !getFlags().get(ActorFlag::Swimming);
}

bool ServerPlayer::attackActor(ServerNetworkHandler &owner, uint64_t targetRuntimeId) {
    ActorEventPacket swing;
    swing.mRuntimeActorId = getRuntimeId();
    swing.mEventId = (uint8_t) EntityEventType::ArmSwing;
    swing.mEventData = 0;
    swing.mHasFirePosition = false;
    for (const auto &entry: owner.getPlayers()) {
        if (&entry.second != this && entry.second.isSpawned() && entry.second.getDimension() == getDimension())
            owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), swing, owner.getCodecContext());
    }

    if (!isSpawned() || isDead() || getGameType() == (int32_t) GameType::Spectator)
        return false;

    ServerPlayer *victim = nullptr;
    for (auto &entry: owner.getPlayers()) {
        if (entry.second.getRuntimeId() == targetRuntimeId) {
            victim = &entry.second;
            break;
        }
    }

    if (victim == nullptr) {
        for (auto &entry: owner.getActors()) {
            ServerActor &target = *entry.second;
            if (target.getRuntimeId() != targetRuntimeId || !target.isAlive() || target.isProjectile() ||
                target.getDimension() != getDimension())
                continue;

            const Vector3f actorDelta(target.getPosition().x - getPosition().x,
                                      target.getPosition().y - getPosition().y,
                                      target.getPosition().z - getPosition().z);
            if (actorDelta.x * actorDelta.x + actorDelta.y * actorDelta.y + actorDelta.z * actorDelta.z >
                MAX_REACH * MAX_REACH)
                return false;

            owner.getScriptEngine().onEntityHitEntity(*this, target);
            if (!target.isAlive())
                return false;

            const ItemStack &weapon = getInventory().getItemInHand();
            const ItemData *weaponData = weapon.isAir() || weapon.mDefinition == nullptr
                                                 ? nullptr
                                                 : ItemDataTable::find(weapon.mDefinition->getIdentifier());
            const Item *weaponType = weapon.isAir() || weapon.mDefinition == nullptr
                                             ? nullptr
                                             : VanillaItems::fromIdentifier(weapon.mDefinition->getIdentifier());
            const float weaponBaseDamage = weaponData == nullptr || weaponData->mAttackDamage <= 0
                                                   ? 1.0f
                                                   : (float) weaponData->mAttackDamage;
            float attackDamage = weaponBaseDamage;
            if (weaponType != nullptr)
                attackDamage = std::max(0.0f, attackDamage + weaponType->getAttackDamageBonus(weapon, *this));

            attackDamage = _applyAttackerModifiers(weaponBaseDamage,
                                                   attackDamage + target.getMeleeEnchantmentBonus(weapon));
            if (attackDamage <= 0.0f)
                return false;

            owner.damageActor(target, attackDamage, this);
            if (target.isAlive())
                target.onMeleeEnchantmentHit(weapon);
            if (weaponType != nullptr)
                weaponType->onPostAttack(owner, *this, target, attackDamage, weapon);

            const int32_t weaponWear = (weaponData != nullptr && weaponData->mToolType == ToolType::Sword) ? 1 : 2;
            owner.damagePlayerHeldItem(*this, weaponWear);
            return true;
        }
    }

    if (victim == nullptr || victim == this || !victim->isSpawned() || victim->isDead() ||
        victim->getGameType() == (int32_t) GameType::Spectator ||
        victim->getGameType() == (int32_t) GameType::Creative)
        return false;

    if (!owner.getLevelFor(*this).getGameRules().getBool("pvp"))
        return false;

    const Vector3f delta(victim->getPosition().x - getPosition().x,
                         victim->getPosition().y - getPosition().y,
                         victim->getPosition().z - getPosition().z);
    if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z > MAX_REACH * MAX_REACH)
        return false;

    owner.getScriptEngine().onEntityHitEntity(*this, *victim);
    if (victim->isDead())
        return false;

    const ItemStack &held = getInventory().getItemInHand();
    const ItemData *data = held.isAir() || held.mDefinition == nullptr
                           ? nullptr : ItemDataTable::find(held.mDefinition->getIdentifier());
    const Item *heldType = held.isAir() || held.mDefinition == nullptr
                                   ? nullptr : VanillaItems::fromIdentifier(held.mDefinition->getIdentifier());
    const float baseDamage = data == nullptr || data->mAttackDamage <= 0 ? 1.0f : (float) data->mAttackDamage;
    float damage = baseDamage;

    if (heldType != nullptr)
        damage = std::max(0.0f, damage + heldType->getAttackDamageBonus(held, *this));

    damage = _applyAttackerModifiers(baseDamage, damage + victim->getMeleeEnchantmentBonus(held));
    if (damage <= 0.0f)
        return false;

    const bool coldTarget = victim->getAttackTime() <= 0;
    const float rawDamage = damage;
    if (victim->getNoDamageTicks() > 0) {
        if (victim->getLastDamageAmount() >= damage)
            return false;
        damage -= victim->getLastDamageAmount();
    }

    const float armorEfficiency = heldType == nullptr ? 1.0f : heldType->getArmorEfficiency(held);
    const float finalDamage = armorReducedDamage(*victim, damage, armorEfficiency);
    if (finalDamage <= 0.0f)
        return false;

    owner.applyDamage(*victim, finalDamage, "death.attack.player", {victim->getName(), getName()}, false, false);
    if (heldType != nullptr)
        heldType->onPostAttack(owner, *this, *victim, damage, held);

    const int32_t weaponWear = (data != nullptr && data->mToolType == ToolType::Sword) ? 1 : 2;
    owner.damagePlayerHeldItem(*this, weaponWear);

    if (victim->isDead())
        return true;

    victim->setNoDamageTicks(10);
    victim->setLastDamageAmount(rawDamage);
    if (coldTarget)
        victim->setAttackTime(ATTACK_COOLDOWN_TICKS);

    if (coldTarget) {
        victim->knockBack(delta.x, delta.z, 0.4f);
        const int knockback = ItemEnchantments::getLevel(held, EnchantmentIds::KNOCKBACK);
        if (knockback > 0)
            victim->knockBack(delta.x, delta.z, 0.5f * (float) knockback);
        sendMotion(owner, *victim);
    }

    const int fireAspect = ItemEnchantments::getLevel(held, EnchantmentIds::FIRE_ASPECT);
    const bool wasOnFire = victim->isOnFire();
    if (fireAspect > 0 && !victim->hasEffect(MobEffectId::FireResistance))
        victim->setFireTicks(std::max(victim->getFireTicks(), fireAspect * 4 * 20));
    if (wasOnFire != victim->isOnFire())
        owner._sendEntityData(*victim);

    damageArmor(owner, *victim, damage);

    int thornsDamage = 0;
    for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; ++slot) {
        ItemStack armor = victim->getInventory().getArmor(slot);
        const int thorns = ItemEnchantments::getLevel(armor, EnchantmentIds::THORNS);
        if (thorns <= 0)
            continue;

        int itemDamage = 1;
        if ((std::rand() % 100) < std::min(100, thorns * 15)) {
            itemDamage = 3;
            thornsDamage += thorns > 10 ? thorns - 10 : 1 + std::rand() % 4;
        }
        if (damageItem(armor, itemDamage))
            victim->getInventory().setArmor(slot, std::move(armor));
    }
    if (thornsDamage > 0)
        owner.applyDamage(*this, (float) thornsDamage, "death.attack.thorns", {getName(), victim->getName()}, false,
                          false);
    victim->getInventoryManager().syncContents(InventoryManager::InventoryId::Armor);
    InventoryHandler::sendArmorContent(owner, *victim);

    damageHeldItem(*this);
    exhaust(0.1f);
    owner._sendAttributes(*this);

    if (_isCriticalHit())
        broadcastAnimation(owner, *victim, AnimatePacket::Action::CriticalHit);
    if (victim->getMeleeEnchantmentBonus(held) > 0.0f)
        broadcastAnimation(owner, *victim, AnimatePacket::Action::MagicCriticalHit);
    return true;
}

void ServerPlayer::startSpinAttack(ServerNetworkHandler &owner, int32_t ticks) {
    mSpinAttackTicks = ticks;
    getFlags().set(ActorFlag::SpinAttack, true);
    resetFallDistance();
    owner._sendEntityData(*this);
}

void ServerPlayer::tickSpinAttack(ServerNetworkHandler &owner) {
    if (mSpinAttackTicks <= 0)
        return;

    resetFallDistance();

    if (isDead()) {
        mSpinAttackTicks = 0;
        getFlags().set(ActorFlag::SpinAttack, false);
        owner._sendEntityData(*this);
        return;
    }

    const Vector3f position = getPosition();

    const auto reaches = [&position](const Vector3f &target, float width, float height) {
        const float halfWidth = width * 0.5f + PLAYER_WIDTH * 0.5f + SPIN_ATTACK_REACH;
        if (std::fabs(target.x - position.x) > halfWidth || std::fabs(target.z - position.z) > halfWidth)
            return false;
        return target.y + height >= position.y && target.y <= position.y + PLAYER_HEIGHT;
    };

    for (auto &entry: owner.getActors()) {
        ServerActor &target = *entry.second;
        if (!target.isAlive() || target.isDead() || target.isProjectile() || target.getNoDamageTicks() > 0 ||
            target.getDimension() != getDimension())
            continue;

        const ActorSize size = target.getSize();
        if (reaches(target.getPosition(), size.mWidth, size.mHeight))
            owner.damageActor(target, SPIN_ATTACK_DAMAGE, this);
    }

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &target = entry.second;
        if (&target == this || !target.isSpawned() || target.isDead() || target.getNoDamageTicks() > 0)
            continue;
        if (target.getGameType() == (int32_t) GameType::Creative
            || target.getGameType() == (int32_t) GameType::Spectator)
            continue;

        if (reaches(target.getPosition(), PLAYER_WIDTH, PLAYER_HEIGHT))
            owner.applyDamage(target, SPIN_ATTACK_DAMAGE, "death.attack.player",
                              {target.getName(), getName()}, false, false);
    }

    --mSpinAttackTicks;
    if (mSpinAttackTicks <= 0) {
        getFlags().set(ActorFlag::SpinAttack, false);
        owner._sendEntityData(*this);
    }
}

void ServerPlayer::teleport(ServerNetworkHandler &owner, const Vector3f &position,
                             MovePlayerTeleportationCause cause) {
    Actor::teleport(position);
    markTeleported();
    clearPendingMove();

    MovePlayerPacket packet;
    packet.mRuntimeActorId = getUniqueId();
    packet.mPosition = Vector3f(getPosition().x, getPosition().y + 1.62f, getPosition().z);
    packet.mRotation = getRotation();
    packet.mMode = MovePlayerMode::Teleport;
    packet.mOnGround = isOnGround();
    packet.mTeleportationCause = cause;
    packet.mActorType = 0;
    packet.mTick = owner.getCurrentTick();
    owner.getNetworkHandler().send(getNetworkIdentifier(), packet, owner.getCodecContext());

    ChunkStreamHandler::handleTeleport(owner, *this);
}

void ServerPlayer::syncEffects() {
    if (mSender == nullptr)
        return;
    for (const auto &entry: getEffects().getAll()) {
        const MobEffectInstance &effect = entry.second;
        MobEffectPacket packet;
        packet.mRuntimeActorId = getRuntimeId();
        packet.mEvent = MobEffectPacket::Event::Add;
        packet.mEffectId = (int32_t) effect.mId;
        packet.mAmplifier = effect.mAmplifier;
        packet.mParticles = effect.mParticles;
        packet.mDuration = effect.mDuration;
        packet.mAmbient = effect.mAmbient;
        mSender->sendPacketTo(mId, packet);
    }
}

bool ServerPlayer::hasItemCooldown(const ItemStack &item, int64_t currentTick) const {
    return getItemCooldownRemaining(item, currentTick) > 0;
}

int ServerPlayer::getItemCooldownRemaining(const ItemStack &item, int64_t currentTick) const {
    if (item.isAir() || item.mDefinition == nullptr)
        return 0;

    const CooldownItemComponent *cooldown = ItemDataTable::getComponents(item.mDefinition->getIdentifier())
                                                    .get<CooldownItemComponent>();
    const std::string category = (cooldown != nullptr && !cooldown->getCategory().empty())
                                         ? cooldown->getCategory()
                                         : item.mDefinition->getIdentifier();

    const auto it = mItemCooldowns.find(category);
    if (it == mItemCooldowns.end() || it->second <= currentTick)
        return 0;

    const int64_t remaining = it->second - currentTick;
    return remaining > std::numeric_limits<int>::max() ? std::numeric_limits<int>::max() : (int) remaining;
}

void ServerPlayer::startItemCooldown(const ItemStack &item, int64_t currentTick, int ticks) {
    if (item.isAir() || item.mDefinition == nullptr)
        return;

    const CooldownItemComponent *cooldown = ItemDataTable::getComponents(item.mDefinition->getIdentifier())
                                                    .get<CooldownItemComponent>();
    const std::string category = (cooldown != nullptr && !cooldown->getCategory().empty())
                                         ? cooldown->getCategory()
                                         : item.mDefinition->getIdentifier();
    const int duration = ticks > 0 ? ticks : (cooldown != nullptr ? cooldown->getDurationTicks() : 0);
    if (duration <= 0)
        return;

    mItemCooldowns[category] = currentTick + duration;
    if (mSender == nullptr)
        return;

    PlayerStartItemCooldownPacket packet;
    packet.mItemCategory = category;
    packet.mCooldownDuration = duration;
    mSender->sendPacketTo(mId, packet);
}

void ServerPlayer::tickItemCooldowns(int64_t currentTick) {
    for (auto it = mItemCooldowns.begin(); it != mItemCooldowns.end();) {
        if (it->second <= currentTick)
            it = mItemCooldowns.erase(it);
        else
            ++it;
    }
}

void ServerPlayer::consumeOneHeldItem() {
    if (getGameType() == (int32_t) GameType::Creative)
        return;

    ItemStack held = mInventory.getItemInHand();
    if (held.isAir())
        return;

    held.mCount -= 1;
    if (held.mCount <= 0)
        mInventory.setItemInHand(ItemStack::air());
    else
        mInventory.setItemInHand(std::move(held));

    getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, mInventory.getSelectedSlot());
}

void ServerPlayer::sendMessage(const std::string &message) {
    if (mSender == nullptr)
        return;

    TextPacket text;
    text.mType = TextPacket::Type::Raw;
    text.mMessage = message;
    mSender->sendPacketTo(mId, text);
}

void ServerPlayer::sendTranslation(const std::string &key, const std::vector<std::string> &parameters) {
    if (mSender == nullptr)
        return;

    TextPacket text;
    text.mType = TextPacket::Type::Translation;
    text.mMessage = key;
    text.mParameters = parameters;
    text.mNeedsTranslation = true;
    mSender->sendPacketTo(mId, text);
}

void ServerPlayer::sendTip(const std::string &message) {
    if (mSender == nullptr)
        return;

    TextPacket text;
    text.mType = TextPacket::Type::Tip;
    text.mMessage = message;
    mSender->sendPacketTo(mId, text);
}

void ServerPlayer::sendPopup(const std::string &message) {
    if (mSender == nullptr)
        return;

    TextPacket text;
    text.mType = TextPacket::Type::Popup;
    text.mMessage = message;
    mSender->sendPacketTo(mId, text);
}

void ServerPlayer::sendTitle(const std::string &title, const std::string &subtitle, int32_t fadeInTime,
                             int32_t stayTime, int32_t fadeOutTime) {
    if (mSender == nullptr)
        return;

    SetTitlePacket times;
    times.mType = SetTitlePacket::Type::Times;
    times.mFadeInTime = fadeInTime;
    times.mStayTime = stayTime;
    times.mFadeOutTime = fadeOutTime;
    mSender->sendPacketTo(mId, times);

    if (!subtitle.empty()) {
        SetTitlePacket subtitlePacket;
        subtitlePacket.mType = SetTitlePacket::Type::Subtitle;
        subtitlePacket.mText = subtitle;
        mSender->sendPacketTo(mId, subtitlePacket);
    }

    SetTitlePacket titlePacket;
    titlePacket.mType = SetTitlePacket::Type::Title;
    titlePacket.mText = title;
    mSender->sendPacketTo(mId, titlePacket);
}

void ServerPlayer::sendTitleText(const std::string &text, bool json) {
    if (mSender == nullptr)
        return;

    SetTitlePacket packet;
    packet.mType = json ? SetTitlePacket::Type::TitleJson : SetTitlePacket::Type::Title;
    packet.mText = text;
    mSender->sendPacketTo(mId, packet);
}

void ServerPlayer::sendSubtitle(const std::string &text, bool json) {
    if (mSender == nullptr)
        return;

    SetTitlePacket packet;
    packet.mType = json ? SetTitlePacket::Type::SubtitleJson : SetTitlePacket::Type::Subtitle;
    packet.mText = text;
    mSender->sendPacketTo(mId, packet);
}

void ServerPlayer::sendActionBar(const std::string &text, bool json) {
    if (mSender == nullptr)
        return;

    SetTitlePacket packet;
    packet.mType = json ? SetTitlePacket::Type::ActionbarJson : SetTitlePacket::Type::Actionbar;
    packet.mText = text;
    mSender->sendPacketTo(mId, packet);
}

void ServerPlayer::setTitleTimes(int32_t fadeInTime, int32_t stayTime, int32_t fadeOutTime) {
    if (mSender == nullptr)
        return;

    SetTitlePacket packet;
    packet.mType = SetTitlePacket::Type::Times;
    packet.mFadeInTime = fadeInTime;
    packet.mStayTime = stayTime;
    packet.mFadeOutTime = fadeOutTime;
    mSender->sendPacketTo(mId, packet);
}

void ServerPlayer::clearTitle() {
    if (mSender == nullptr)
        return;

    SetTitlePacket packet;
    packet.mType = SetTitlePacket::Type::Clear;
    mSender->sendPacketTo(mId, packet);
}

void ServerPlayer::resetTitle() {
    if (mSender == nullptr)
        return;

    SetTitlePacket packet;
    packet.mType = SetTitlePacket::Type::Reset;
    mSender->sendPacketTo(mId, packet);
}

Tag ServerPlayer::saveNbt(const std::string &levelName) const {
    Tag data = Tag::ofCompound();

    data.put(TAG_POS, floatList(mPosition.x, mPosition.y, mPosition.z));
    data.put(TAG_MOTION, floatList(mMotion.x, mMotion.y, mMotion.z));
    data.put(TAG_ROTATION, floatList(mRotation.x, mRotation.y, mRotation.z));

    data.putFloat(TAG_HEALTH, mAttributes.get("minecraft:health"));
    data.putInt(TAG_AIR, mAirSupply);
    data.putString(TAG_LEVEL, levelName);
    data.putInt("DimensionId", Dimension::toId(mDimension));
    data.putInt(TAG_GAME_MODE, mGameType);
    data.putLong(TAG_FIRST_PLAYED, mFirstPlayed == 0 ? currentTimeMillis() : mFirstPlayed);
    data.putLong(TAG_LAST_PLAYED, currentTimeMillis());
    data.putString(TAG_LAST_KNOWN_XUID, mXuid);
    data.putString(TAG_NAME, mName);

    if (mHasSpawnPoint) {
        data.putInt(TAG_SPAWN_X, mSpawnPoint.x);
        data.putInt(TAG_SPAWN_Y, mSpawnPoint.y);
        data.putInt(TAG_SPAWN_Z, mSpawnPoint.z);
    }

    data.putInt(TAG_FOOD_LEVEL, (int32_t) getFood());
    data.putFloat(TAG_FOOD_EXHAUSTION_LEVEL, getExhaustion());
    data.putFloat(TAG_FOOD_SATURATION_LEVEL, getSaturation());
    data.putInt(TAG_FOOD_TICK_TIMER, mFoodTickTimer);
    data.putShort(TAG_FIRE, (int16_t) getFireTicks());

    data.putInt(TAG_XP_LEVEL, mExperience.getXpLevel());
    data.putFloat(TAG_XP_PROGRESS, mExperience.getXpProgress());
    data.putInt(TAG_LIFETIME_XP_TOTAL, mExperience.getLifetimeTotalXp());

    Tag inventory = Tag::ofList(Tag::Type::Compound);

    for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; slot++) {
        const ItemStack &item = mInventory.getItem(slot);
        if (!item.isAir())
            inventory.addToList(ItemStackNbt::write(item, slot));
    }

    for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; slot++) {
        const ItemStack &item = mInventory.getArmor(slot);
        if (!item.isAir())
            inventory.addToList(ItemStackNbt::write(item, PlayerInventory::CONTAINER_SIZE + slot));
    }

    data.put(TAG_INVENTORY, inventory);
    data.put(TAG_OFF_INVENTORY, ItemStackNbt::write(mInventory.getOffhand(), 0));
    data.putInt(TAG_SELECTED_SLOT, mInventory.getSelectedSlot());

    const SimpleContainerInventory *enderChest = EnderChestInventoryStore::getInstance().find(getUniqueId());
    if (enderChest != nullptr)
        data.put(TAG_ENDER_ITEMS, enderChest->saveItems());
    else
        data.put(TAG_ENDER_ITEMS, Tag::ofList(Tag::Type::Compound));

    Tag activeEffects = Tag::ofList(Tag::Type::Compound);
    for (const auto &entry: getEffects().getAll()) {
        const MobEffectInstance &effect = entry.second;
        Tag value = Tag::ofCompound();
        value.putByte("Id", (int8_t) effect.mId);
        value.putInt("Duration", effect.mDuration);
        value.putByte("Amplifier", (int8_t) effect.mAmplifier);
        value.putByte("Ambient", effect.mAmbient ? 1 : 0);
        value.putByte("ShowParticles", effect.mParticles ? 1 : 0);
        activeEffects.addToList(std::move(value));
    }
    data.put(TAG_ACTIVE_EFFECTS, activeEffects);

    data.put("DynamicProperties", serializeDynamicProperties(mDynamicProperties));
    saveTags(data);

    return data;
}

void ServerPlayer::loadNbt(const Tag &data, const PacketCodecContext &context) {
    const Tag *dynamicProperties = data.get("DynamicProperties");
    if (dynamicProperties != nullptr)
        deserializeDynamicProperties(*dynamicProperties, mDynamicProperties);

    if (data.getType() != Tag::Type::Compound)
        return;

    loadTags(data);

    mPosition = Vector3f(listValue(data, TAG_POS, 0, mPosition.x),
                         listValue(data, TAG_POS, 1, mPosition.y),
                         listValue(data, TAG_POS, 2, mPosition.z));

    mMotion = Vector3f(listValue(data, TAG_MOTION, 0, 0.0f),
                       listValue(data, TAG_MOTION, 1, 0.0f),
                       listValue(data, TAG_MOTION, 2, 0.0f));

    mRotation = Vector3f(listValue(data, TAG_ROTATION, 0, 0.0f),
                         listValue(data, TAG_ROTATION, 1, 0.0f),
                         listValue(data, TAG_ROTATION, 2, 0.0f));

    mDimension = Dimension::fromId(data.getInt("DimensionId", 0));

    const float storedHealth = data.getFloat(TAG_HEALTH, 20.0f);
    mAttributes.set("minecraft:health", storedHealth);
    mAirSupply = std::clamp(data.getInt(TAG_AIR, MAX_AIR_SUPPLY), -20, MAX_AIR_SUPPLY);
    mGameType = data.getInt(TAG_GAME_MODE, mGameType);
    mFirstPlayed = data.getLong(TAG_FIRST_PLAYED, 0);

    mHasSpawnPoint = data.contains(TAG_SPAWN_X);
    if (mHasSpawnPoint)
        mSpawnPoint = Vector3i(data.getInt(TAG_SPAWN_X, 0), data.getInt(TAG_SPAWN_Y, 0), data.getInt(TAG_SPAWN_Z, 0));
    setFireTicks(std::clamp((int) data.getShort(TAG_FIRE, 0), 0, 32767));

    setFood((float) data.getInt(TAG_FOOD_LEVEL, (int32_t) getFood()));
    setExhaustion(data.getFloat(TAG_FOOD_EXHAUSTION_LEVEL, getExhaustion()));
    setSaturation(data.getFloat(TAG_FOOD_SATURATION_LEVEL, getSaturation()));
    setFoodTickTimer(data.getInt(TAG_FOOD_TICK_TIMER, mFoodTickTimer));

    mExperience.setXpAndProgress(data.getInt(TAG_XP_LEVEL, 0), data.getFloat(TAG_XP_PROGRESS, 0.0f));
    mExperience.setLifetimeTotalXp(data.getInt(TAG_LIFETIME_XP_TOTAL, 0));
    syncExperience();

    mInventory.clear();

    const Tag *inventory = data.get(TAG_INVENTORY);
    if (inventory != nullptr && inventory->getType() == Tag::Type::List) {
        for (const Tag &entry: inventory->getList()) {
            const int slot = ItemStackNbt::readSlot(entry);
            if (slot < 0)
                continue;

            ItemStack item = ItemStackNbt::read(entry, context);
            if (item.isAir())
                continue;

            if (slot < PlayerInventory::CONTAINER_SIZE)
                mInventory.setItem(slot, std::move(item));
            else if (slot < PlayerInventory::SAVED_SIZE)
                mInventory.setArmor(slot - PlayerInventory::CONTAINER_SIZE, std::move(item));
        }
    }

    const Tag *offhand = data.get(TAG_OFF_INVENTORY);
    if (offhand != nullptr && offhand->getType() == Tag::Type::Compound)
        mInventory.setOffhand(ItemStackNbt::read(*offhand, context));

    mInventory.setSelectedSlot(data.getInt(TAG_SELECTED_SLOT, 0));

    SimpleContainerInventory &enderChest = EnderChestInventoryStore::getInstance().get(getUniqueId());
    enderChest.clear();

    const Tag *enderItems = data.get(TAG_ENDER_ITEMS);
    if (enderItems != nullptr && enderItems->getType() == Tag::Type::List)
        enderChest.loadItems(*enderItems, context);

    getEffects().clear();
    const Tag *activeEffects = data.get(TAG_ACTIVE_EFFECTS);
    if (activeEffects != nullptr && activeEffects->getType() == Tag::Type::List) {
        for (const Tag &value: activeEffects->getList()) {
            if (value.getType() != Tag::Type::Compound)
                continue;
            const int id = value.getByte("Id", 0);
            if (id < 1 || id > 30)
                continue;
            MobEffectInstance effect;
            effect.mId = (MobEffectId) id;
            effect.mDuration = value.getInt("Duration", 0);
            effect.mAmplifier = value.getByte("Amplifier", 0);
            effect.mAmbient = value.getByte("Ambient", 0) != 0;
            effect.mParticles = value.getByte("ShowParticles", 1) != 0;
            effect.mInfinite = effect.mDuration < 0;
            addEffect(effect);
        }
    }
    mAttributes.setClamped("minecraft:health", storedHealth);
}
