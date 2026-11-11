#include "World/Actor/Misc/ArmorStandActor.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/ArmorStandItem.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Block/Systems/FireBlocksFetch.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <random>

FALCON_REGISTER_ACTOR(ArmorStandActor, ArmorStandActor::IDENTIFIER);

namespace {
    constexpr int32_t ACTOR_DATA_POSE_INDEX = 79;
    constexpr int32_t ACTOR_DATA_STRUCTURAL_INTEGRITY = 1;
    constexpr int32_t ACTOR_DATA_HURT_TIME = 11;
    constexpr int32_t LEVEL_EVENT_PARTICLE_DESTROY = 2017;
    constexpr int32_t SHAKE_HURT_TIME = 10;
    constexpr int32_t SHAKE_STRUCTURAL_INTEGRITY = -10;
    constexpr const char *TAG_POSE = "Pose";
    constexpr const char *TAG_POSE_INDEX = "PoseIndex";
    constexpr const char *TAG_LAST_SIGNAL = "LastSignal";
    constexpr const char *ITEM_LOCK_TAG = "minecraft:item_lock";
    constexpr const char *DROP_RULE = "dotiledrops";
    constexpr const char *GRIEFING_RULE = "mobgriefing";
    constexpr int64_t DOUBLE_HIT_TICKS = 5;
    constexpr int32_t FIRE_CONTACT_IGNITE_TICKS = 5 * 20;
    constexpr float FIRE_CONTACT_DAMAGE = 0.05f;
    constexpr float FIRE_TICK_DAMAGE = 1.0f;
    constexpr float MAX_INTERACT_DISTANCE = 8.0f;

    std::mt19937 &dropRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    Vector3f scatteredDropPosition(const Vector3f &position) {
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        std::mt19937 &random = dropRandom();
        const float x = unit(random) * 0.5f - 0.25f;
        const float y = unit(random) * 1.5f + 0.25f;
        const float z = unit(random) * 0.5f - 0.25f;
        return Vector3f(position.x + x, position.y + y, position.z + z);
    }

    const Item *itemTypeOf(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return nullptr;
        return VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
    }

    bool isSameStack(const ItemStack &left, const ItemStack &right) {
        return left.mDefinition == right.mDefinition && left.mCount == right.mCount && left.mDamage == right.mDamage
               && left.mTag == right.mTag;
    }

    int slotOfArmor(ArmorSlot slot) {
        switch (slot) {
            case ArmorSlot::Head:
                return MobEquipment::HEAD;
            case ArmorSlot::Chest:
                return MobEquipment::CHEST;
            case ArmorSlot::Legs:
                return MobEquipment::LEGS;
            case ArmorSlot::Feet:
                return MobEquipment::FEET;
            default:
                return MobEquipment::MAINHAND;
        }
    }
}

ArmorStandActor::ArmorStandActor(uint64_t runtimeId, const std::string &identifier)
        : ServerActor(runtimeId, identifier) {
    getFlags().set(ActorFlag::HasGravity, true);
}

int ArmorStandActor::slotForClick(float offsetY, bool mainhandOccupied) {
    if (!(offsetY >= 0.1f))
        return -1;
    if (offsetY < 0.55f)
        return MobEquipment::FEET;
    if (offsetY >= 0.9f && offsetY < 1.5999999f)
        return mainhandOccupied ? MobEquipment::MAINHAND : MobEquipment::CHEST;
    if (offsetY < 1.2f)
        return MobEquipment::LEGS;
    return offsetY >= 1.6f ? MobEquipment::HEAD : -1;
}

bool ArmorStandActor::exchange(ItemStack &slot, ItemStack &held, bool creative) {
    if (slot.isAir() && held.isAir())
        return false;

    if (!slot.isAir() && !held.isAir() && (held.mCount >= 2 || isSameStack(slot, held)))
        return false;

    if (!held.isAir() && slot.isAir() && (creative || held.mCount >= 2)) {
        slot = held;
        slot.mCount = 1;
        if (!creative)
            held.mCount--;
        return true;
    }

    std::swap(slot, held);
    return true;
}

void ArmorStandActor::tick(ServerNetworkHandler &owner) {
    if (mExpired)
        return;

    if (getMaxHealth() != MAX_HEALTH) {
        setMaxHealth(MAX_HEALTH);
        setHealth(MAX_HEALTH);
    }

    ServerActor::tick(owner);
    if (mExpired)
        return;

    _tickShake(owner);
    _tickFireContact(owner);
    if (!mExpired)
        _tickRedstone(owner);
}

void ArmorStandActor::_tickShake(ServerNetworkHandler &owner) {
    if (mHurtTime <= 0)
        return;

    mHurtTime--;

    EntityDataMap metadata;
    EntityDataEntry hurtTime;
    hurtTime.mId = ACTOR_DATA_HURT_TIME;
    hurtTime.mFormat = EntityDataFormat::Int;
    hurtTime.mIntValue = mHurtTime;
    metadata.mEntries.push_back(hurtTime);
    owner.sendActorMetadata(*this, metadata);
}

void ArmorStandActor::_shake(ServerNetworkHandler &owner) {
    // the stand wobbles through its hurt time instead of the mob hurt animation
    mHurtTime = SHAKE_HURT_TIME;

    EntityDataMap metadata;
    EntityDataEntry hurtTime;
    hurtTime.mId = ACTOR_DATA_HURT_TIME;
    hurtTime.mFormat = EntityDataFormat::Int;
    hurtTime.mIntValue = mHurtTime;
    metadata.mEntries.push_back(hurtTime);

    EntityDataEntry integrity;
    integrity.mId = ACTOR_DATA_STRUCTURAL_INTEGRITY;
    integrity.mFormat = EntityDataFormat::Int;
    integrity.mIntValue = SHAKE_STRUCTURAL_INTEGRITY;
    metadata.mEntries.push_back(integrity);
    owner.sendActorMetadata(*this, metadata);
}

void ArmorStandActor::_break(ServerNetworkHandler &owner, bool drops) {
    if (mExpired)
        return;

    Level &level = owner.getLevelFor(*this);
    const Vector3f position = getPosition();
    owner.broadcastLevelEvent(level, LEVEL_EVENT_PARTICLE_DESTROY, position, (int32_t) getSize().mHeight);
    owner.broadcastLevelEvent(level, LEVEL_EVENT_BREAK, position, 0);
    _destroy(owner, drops, drops);
}

void ArmorStandActor::_tickFireContact(ServerNetworkHandler &owner) {
    const FireContact contact = FireBlocksFetch::at(owner.getLevelFor(*this), getPosition());
    if (!contact.fire)
        return;

    if (!isOnFire()) {
        setFireTicks(FIRE_CONTACT_IGNITE_TICKS);
        setOnFire(true);
        owner.syncActorFlags(*this);
    }
    _burn(owner, FIRE_CONTACT_DAMAGE);
}

void ArmorStandActor::_tickRedstone(ServerNetworkHandler &owner) {
    Level &level = owner.getLevelFor(*this);
    const Vector3f position = getPosition();
    const Vector3i feet((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));

    int32_t signal = RedstoneSystem::isBlockIndirectlyGettingPowered(owner, level, feet);
    if (signal <= 0)
        signal = RedstoneSystem::isBlockIndirectlyGettingPowered(owner, level, Vector3i(feet.x, feet.y - 1, feet.z));

    if (signal > 0 && signal != mLastSignal) {
        const int32_t pose = std::min(signal, POSE_COUNT) - 1;
        if (pose != mPoseIndex)
            _setPose(owner, pose);
    }
    mLastSignal = std::max(signal, 0);
}

void ArmorStandActor::_setPose(ServerNetworkHandler &owner, int32_t poseIndex) {
    mPoseIndex = poseIndex;

    EntityDataMap metadata;
    EntityDataEntry pose;
    pose.mId = ACTOR_DATA_POSE_INDEX;
    pose.mFormat = EntityDataFormat::Int;
    pose.mIntValue = mPoseIndex;
    metadata.mEntries.push_back(pose);
    owner.sendActorMetadata(*this, metadata);
}

void ArmorStandActor::_resync(ServerNetworkHandler &owner, ServerPlayer &player) const {
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                          player.getInventory().getSelectedSlot());
    if (!mExpired)
        mEquipment.sendAllTo(owner, player, *this);
}

bool ArmorStandActor::_canInteract(ServerNetworkHandler &owner, ServerPlayer &player,
                                   const Vector3f &clickPosition) const {
    if (mExpired || isDead() || !player.isSpawned() || player.isDead() || !player.sharesLevelWith(*this))
        return false;

    if (player.getGameType() == (int32_t) GameType::Spectator)
        return false;

    if (!std::isfinite(clickPosition.x) || !std::isfinite(clickPosition.y) || !std::isfinite(clickPosition.z))
        return false;

    const Vector3f &from = player.getPosition();
    const Vector3f &to = getPosition();
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    if (dx * dx + dy * dy + dz * dz > MAX_INTERACT_DISTANCE * MAX_INTERACT_DISTANCE)
        return false;

    // one transaction per click: a repeat within the tick is a duplicate undoing the exchange it follows
    return owner.getCurrentTick() != mLastInteractTick;
}

int ArmorStandActor::_slotFor(const ItemStack &held, float offsetY) const {
    const int clicked = slotForClick(offsetY, !mEquipment.getSlot(MobEquipment::MAINHAND).isAir());
    if (clicked < 0 || held.isAir())
        return clicked;

    const Item *type = itemTypeOf(held);
    if (type == nullptr || dynamic_cast<const ArmorStandItem *>(type) != nullptr)
        return -1;

    const Block *block = VanillaBlocks::fromIdentifier(held.mDefinition->getIdentifier());
    if (block != nullptr && block->isWornOnHead())
        return MobEquipment::HEAD;

    return slotOfArmor(type->getArmorSlot());
}

bool ArmorStandActor::onInteractAt(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &clickPosition) {
    if (!_canInteract(owner, player, clickPosition)) {
        _resync(owner, player);
        return false;
    }

    mLastInteractTick = owner.getCurrentTick();

    if (player.getFlags().get(ActorFlag::Sneaking)) {
        _setPose(owner, (mPoseIndex + 1) % POSE_COUNT);
        return true;
    }

    PlayerInventory &inventory = player.getInventory();
    ItemStack held = inventory.getItemInHand();

    const Tag *lock = held.isAir() ? nullptr : held.mTag.get(ITEM_LOCK_TAG);
    const int slot = lock != nullptr && lock->asInt() != 0 ? -1 : _slotFor(held, clickPosition.y - getPosition().y);
    if (slot < 0) {
        _resync(owner, player);
        return false;
    }

    ItemStack worn = mEquipment.getSlot(slot);
    if (!exchange(worn, held, player.getGameType() == (int32_t) GameType::Creative)) {
        _resync(owner, player);
        return false;
    }

    mEquipment.setSlot(slot, std::move(worn));
    inventory.setItemInHand(std::move(held));

    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    InventoryHandler::sendHeldItem(owner, player);
    mEquipment.broadcast(owner, *this);
    owner.playLevelSound(owner.getLevelFor(*this), LevelSoundEvent::ARMOR_EQUIP_GENERIC, getPosition(), IDENTIFIER, -1);
    return true;
}

bool ArmorStandActor::senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) {
    (void) owner;
    (void) amount;

    // onHurt only receives the attacking player; the stand reacts to the damage kind
    const std::string &key = source.mDeathMessageKey;
    mPendingProjectile = source.mProjectile;
    mPendingMobAttacker = source.mAttacker != nullptr && !source.mAttacker->isPlayer();

    if (source.mAttacker != nullptr)
        mPendingHit = HitKind::Attack;
    else if (key == "death.attack.explosion")
        mPendingHit = HitKind::Explosion;
    else if (key == "death.attack.inFire" || key == "death.attack.onFire")
        mPendingHit = HitKind::Fire;
    else if (key == "death.attack.outOfWorld")
        mPendingHit = HitKind::Void;
    else if (DamageCause::matches("fall", key))
        mPendingHit = HitKind::Fall;
    else
        mPendingHit = HitKind::Other;

    return true;
}

bool ArmorStandActor::onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) {
    (void) amount;

    if (mExpired)
        return true;

    switch (mPendingHit) {
        case HitKind::Attack:
            _onAttacked(owner, source);
            break;
        case HitKind::Explosion:
            _destroy(owner, false, true);
            break;
        case HitKind::Void:
            _destroy(owner, false, false);
            break;
        case HitKind::Fall:
            owner.broadcastLevelEvent(owner.getLevelFor(*this), LEVEL_EVENT_LAND, getPosition(), 0);
            break;
        case HitKind::Fire: {
            // standing in fire or lava burns through the contact damage instead
            Level &level = owner.getLevelFor(*this);
            if (!FireBlocksFetch::at(level, getPosition()).fire && !LiquidBlocksFetch::at(level, getPosition()).lava)
                _burn(owner, FIRE_TICK_DAMAGE);
            break;
        }
        case HitKind::Other:
            break;
    }
    return true;
}

void ArmorStandActor::_onAttacked(ServerNetworkHandler &owner, ServerPlayer *source) {
    Level &level = owner.getLevelFor(*this);

    // only a player or a projectile reaches the stand; a mob's own projectile needs mob griefing
    if (source == nullptr) {
        if (mPendingMobAttacker && mPendingProjectile && level.getGameRules().getBool(GRIEFING_RULE))
            _break(owner, true);
        return;
    }

    const int32_t gameType = source->getGameType();
    if (gameType == (int32_t) GameType::Creative) {
        _break(owner, false);
        return;
    }

    if (gameType == (int32_t) GameType::Adventure)
        return;

    owner.broadcastLevelEvent(level, LEVEL_EVENT_HIT, getPosition(), 0);

    const int64_t tick = owner.getCurrentTick();
    if (tick - mLastHitTick > DOUBLE_HIT_TICKS) {
        mLastHitTick = tick;
        _shake(owner);
    } else {
        _break(owner, true);
    }

    if (mPendingProjectile)
        _break(owner, true);
}

void ArmorStandActor::_burn(ServerNetworkHandler &owner, float amount) {
    if (getHealth() - amount <= 0.0f) {
        _destroy(owner, false, true);
        return;
    }

    setHealth(getHealth() - amount);
    owner.syncActorAttributes(*this);
}

void ArmorStandActor::_destroy(ServerNetworkHandler &owner, bool dropStand, bool dropEquipment) {
    // set before anything drops: later hits, explosions or clicks this tick stop here, and the actor is no longer
    // saved: nothing exists both as a drop and on the stand
    if (mExpired)
        return;
    mExpired = true;

    Level &level = owner.getLevelFor(*this);
    const bool drops = level.getGameRules().getBool(DROP_RULE);
    const Vector3f position = getPosition();

    for (int slot = 0; slot < MobEquipment::SLOT_COUNT; ++slot) {
        ItemStack item = mEquipment.getSlot(slot);
        mEquipment.setSlot(slot, ItemStack::air());
        if (dropEquipment && drops && !item.isAir())
            owner.dropItem(level, scatteredDropPosition(position), item, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    }

    if (dropStand && drops)
        owner.spawnItemActor(level, IDENTIFIER, 1, Vector3f(position.x, position.y + 1.0f, position.z));
}

void ArmorStandActor::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t lootingLevel) {
    (void) source;
    (void) lootingLevel;

    _destroy(owner, false, true);
}

void ArmorStandActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    EntityDataEntry flags;
    flags.mId = ActorFlags::FLAGS_DATA_ID;
    flags.mFormat = EntityDataFormat::Long;
    flags.mLongValue = getFlags().getLowBits();
    metadata.mEntries.push_back(flags);

    EntityDataEntry flags2;
    flags2.mId = ActorFlags::FLAGS_2_DATA_ID;
    flags2.mFormat = EntityDataFormat::Long;
    flags2.mLongValue = getFlags().getHighBits();
    metadata.mEntries.push_back(flags2);


    EntityDataEntry flags3;

    flags3.mId = ActorFlags::FLAGS_3_DATA_ID;

    flags3.mFormat = EntityDataFormat::Long;

    flags3.mLongValue = getFlags().getExtendedBits();
metadata.mEntries.push_back(flags3);


    EntityDataEntry nameplateDistance;

    nameplateDistance.mId = ActorFlags::NAMEPLATE_RENDER_DISTANCE_DATA_ID;

    nameplateDistance.mFormat = EntityDataFormat::Float;

    nameplateDistance.mFloatValue = getNameplateRenderDistance();

    metadata.mEntries.push_back(nameplateDistance);

    EntityDataEntry pose;
    pose.mId = ACTOR_DATA_POSE_INDEX;
    pose.mFormat = EntityDataFormat::Int;
    pose.mIntValue = mPoseIndex;
    metadata.mEntries.push_back(pose);
}

void ArmorStandActor::sendEquipmentTo(ServerNetworkHandler &owner, const ServerPlayer &player) const {
    mEquipment.sendTo(owner, player, *this);
}

void ArmorStandActor::loadEquipment(const Tag &data, const PacketCodecContext &context) {
    mEquipment.loadNbt(data, context);
}

Tag ArmorStandActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();
    mEquipment.saveNbt(data);

    Tag pose = Tag::ofCompound();
    pose.putInt(TAG_POSE_INDEX, mPoseIndex);
    pose.putInt(TAG_LAST_SIGNAL, mLastSignal);
    data.put(TAG_POSE, std::move(pose));
    return data;
}

void ArmorStandActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);

    const Tag *pose = data.get(TAG_POSE);
    if (pose == nullptr || !pose->isCompound())
        return;

    mPoseIndex = std::clamp(pose->getInt(TAG_POSE_INDEX, 0), 0, POSE_COUNT - 1);
    mLastSignal = std::max(pose->getInt(TAG_LAST_SIGNAL, 0), 0);
}
