#include "Actor/Mob/MobEquipment.h"

#include "Actor/ArmorProtection.h"
#include "Actor/ActorDamageSource.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Inventory/ItemStackNbt.h"
#include "Inventory/PlayerInventory.h"
#include "Item/Item.h"
#include "Item/Loot/LootItems.h"
#include "Item/Loot/LootTableRegistry.h"
#include "Item/VanillaItems.h"
#include "Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/MobArmorEquipmentPacket.h"
#include "Protocol/Packets/MobEquipmentPacket.h"

#include <algorithm>
#include <random>
#include <utility>
#include <vector>

namespace {
    const char *const EQUIPMENT_COMPONENT = "minecraft:equipment";
    const char *const TAG_MAINHAND = "Mainhand";
    const char *const TAG_OFFHAND = "Offhand";
    const char *const TAG_ARMOR = "Armor";
    const char *const TAG_BODY = "Body";
    const char *const TAG_INVENTORY = "EquippableItems";
    const char *const TAG_CHEST_ITEMS = "ChestItems";
    const char *const SLOT_NAMES[MobEquipment::SLOT_COUNT] = {
            "slot.weapon.mainhand", "slot.weapon.offhand", "slot.armor.head", "slot.armor.chest", "slot.armor.legs",
            "slot.armor.feet", "slot.armor.body"
    };

    const ItemStack &emptyItem() {
        static const ItemStack air = ItemStack::air();
        return air;
    }
    const float DEFAULT_DROP_CHANCE = 0.085f;
    const float LOOTING_DROP_BONUS = 0.01f;
    const int32_t DAMAGE_SPREAD_MARGIN = 3;

    std::mt19937 &equipmentRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    const Item *itemTypeOf(const ItemStack &item) {
        return item.isAir() ? nullptr : VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
    }

    Tag singleItemList(const ItemStack &item) {
        std::vector<Tag> items;
        items.push_back(ItemStackNbt::write(item));
        return Tag::ofList(Tag::Type::Compound, std::move(items));
    }

    const std::vector<Tag> *itemList(const Tag &data, const char *key) {
        const Tag *list = data.get(key);
        if (list == nullptr || list->getType() != Tag::Type::List)
            return nullptr;
        return &list->getList();
    }
}

int MobEquipment::slotIndexOf(const std::string &slotName) {
    for (int slot = 0; slot < SLOT_COUNT; ++slot) {
        if (slotName == SLOT_NAMES[slot])
            return slot;
    }
    return -1;
}

const ItemStack &MobEquipment::getSlot(int slot) const {
    if (slot < 0 || slot >= SLOT_COUNT)
        return emptyItem();

    return mSlots[(size_t) slot];
}

void MobEquipment::setSlot(int slot, ItemStack item) {
    if (slot < 0 || slot >= SLOT_COUNT)
        return;

    mSlots[(size_t) slot] = std::move(item);
}

const ItemStack &MobEquipment::getInventoryItem(int slot) const {
    if (slot < 0 || slot >= (int) mInventory.size())
        return emptyItem();

    return mInventory[(size_t) slot];
}

void MobEquipment::setInventoryItem(int slot, ItemStack item) {
    if (slot < 0)
        return;

    ensureInventorySize(slot + 1);
    mInventory[(size_t) slot] = std::move(item);
}

void MobEquipment::ensureInventorySize(int size) {
    if (size > (int) mInventory.size())
        mInventory.resize((size_t) size, ItemStack::air());
}

void MobEquipment::equipFromTable(ServerNetworkHandler &owner, const MobActor &mob) {
    const json::Value *equipment = mob.getComponent(EQUIPMENT_COMPONENT);
    const json::Value *path = equipment == nullptr ? nullptr : equipment->get("table");
    if (path == nullptr)
        return;

    const LootTable *table = LootTableRegistry::getInstance().get(path->string());
    if (table == nullptr)
        return;

    Level &level = owner.getLevelFor(mob);
    LootContext context(equipmentRandom());
    context.mDifficulty = (int32_t) owner.getProperties().getDifficulty();
    context.mRegionalDifficulty = level.getRegionalDifficulty(context.mDifficulty);

    bool equipped = false;
    for (const LootDrop &drop: table->roll(context)) {
        ItemStack stack = LootItems::toItemStack(owner, drop);
        const int slot = _slotFor(stack);
        if (slot < 0)
            continue;

        mSlots[(size_t) slot] = std::move(stack);
        equipped = true;
    }

    if (equipped)
        broadcast(owner, mob);
}

int MobEquipment::_slotFor(const ItemStack &item) const {
    if (item.isAir())
        return -1;

    const Item *type = itemTypeOf(item);
    if (type != nullptr && type->isArmor())
        return HEAD + (int) type->getArmorSlot() - (int) ArmorSlot::Head;

    if (mSlots[MAINHAND].isAir())
        return MAINHAND;

    if (mSlots[OFFHAND].isAir())
        return OFFHAND;

    return -1;
}

bool MobEquipment::_isEmpty() const {
    return std::all_of(mSlots.begin(), mSlots.end(), [](const ItemStack &item) {
        return item.isAir();
    });
}

void MobEquipment::sendTo(ServerNetworkHandler &owner, const ServerPlayer &player, const Actor &actor) const {
    if (_isEmpty())
        return;

    const int64_t runtimeId = (int64_t) actor.getRuntimeId();
    const NetworkIdentifier &id = player.getNetworkIdentifier();

    MobEquipmentPacket mainhand;
    mainhand.mRuntimeActorId = runtimeId;
    mainhand.mItem = mSlots[MAINHAND];
    mainhand.mInventorySlot = 0;
    mainhand.mHotbarSlot = 0;
    mainhand.mContainerId = PlayerInventory::CONTAINER_ID_INVENTORY;
    owner.getNetworkHandler().send(id, mainhand, owner.getCodecContext());

    MobEquipmentPacket offhand;
    offhand.mRuntimeActorId = runtimeId;
    offhand.mItem = mSlots[OFFHAND];
    offhand.mInventorySlot = PlayerInventory::OFFHAND_NETWORK_SLOT;
    offhand.mHotbarSlot = PlayerInventory::OFFHAND_NETWORK_SLOT;
    offhand.mContainerId = PlayerInventory::CONTAINER_ID_OFFHAND;
    owner.getNetworkHandler().send(id, offhand, owner.getCodecContext());

    MobArmorEquipmentPacket armor;
    armor.mRuntimeActorId = runtimeId;
    armor.mHelmet = mSlots[HEAD];
    armor.mChestplate = mSlots[CHEST];
    armor.mLeggings = mSlots[LEGS];
    armor.mBoots = mSlots[FEET];
    armor.mBody = mSlots[BODY];
    owner.getNetworkHandler().send(id, armor, owner.getCodecContext());
}

void MobEquipment::broadcast(ServerNetworkHandler &owner, const Actor &actor) const {
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.isSpawned() && player.getVisibleActors().count(actor.getRuntimeId()) != 0)
            sendTo(owner, player, actor);
    }
}

float MobEquipment::_dropChance(const MobActor &mob, int slot) {
    const json::Value *equipment = mob.getComponent(EQUIPMENT_COMPONENT);
    const json::Value *chances = equipment == nullptr ? nullptr : equipment->get("slot_drop_chance");
    if (chances == nullptr || !chances->isArray())
        return DEFAULT_DROP_CHANCE;

    for (const std::unique_ptr<json::Value> &entry: chances->mArray) {
        const json::Value *name = entry->get("slot");
        const json::Value *chance = entry->get("drop_chance");
        if (name != nullptr && chance != nullptr && name->string() == SLOT_NAMES[slot])
            return (float) chance->number(DEFAULT_DROP_CHANCE);
    }

    return DEFAULT_DROP_CHANCE;
}

void MobEquipment::_damageForDrop(ItemStack &item) {
    const Item *type = itemTypeOf(item);
    const int32_t maxDurability = type == nullptr ? 0 : type->getMaxDurability();
    if (maxDurability <= 0)
        return;

    const int32_t range = std::max(maxDurability - DAMAGE_SPREAD_MARGIN, 1);
    const int32_t spread = std::uniform_int_distribution<int32_t>(0, range - 1)(equipmentRandom());
    item.mDamage = maxDurability - std::uniform_int_distribution<int32_t>(0, spread)(equipmentRandom());
}

void MobEquipment::dropOnDeath(ServerNetworkHandler &owner, Level &level, const MobActor &mob, bool killedByPlayer,
                               int32_t lootingLevel) {
    for (int slot = 0; slot < SLOT_COUNT; ++slot) {
        ItemStack item = std::move(mSlots[(size_t) slot]);
        mSlots[(size_t) slot] = ItemStack::air();
        if (item.isAir())
            continue;

        const float chance = slot == BODY ? 1.0f : _dropChance(mob, slot);
        const bool guaranteed = chance >= 1.0f;
        if (!guaranteed && !killedByPlayer)
            continue;

        const float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(equipmentRandom());
        if (roll - (float) lootingLevel * LOOTING_DROP_BONUS >= chance)
            continue;

        if (!guaranteed)
            _damageForDrop(item);

        owner.dropItem(level, mob.getPosition(), item, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
    }

    for (ItemStack &slot: mInventory) {
        ItemStack item = std::move(slot);
        slot = ItemStack::air();
        if (!item.isAir())
            owner.dropItem(level, mob.getPosition(), item, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    }
}

void MobEquipment::dropAll(ServerNetworkHandler &owner, Level &level, const Vector3f &position) {
    for (ItemStack &slot: mSlots) {
        ItemStack item = std::move(slot);
        slot = ItemStack::air();
        if (!item.isAir())
            owner.dropItem(level, position, item, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    }

    for (ItemStack &slot: mInventory) {
        ItemStack item = std::move(slot);
        slot = ItemStack::air();
        if (!item.isAir())
            owner.dropItem(level, position, item, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    }
}

bool MobEquipment::dropSlot(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                            const std::string &slotName) {
    for (int slot = 0; slot < SLOT_COUNT; ++slot) {
        if (slotName != SLOT_NAMES[slot] || mSlots[(size_t) slot].isAir())
            continue;

        ItemStack item = std::move(mSlots[(size_t) slot]);
        mSlots[(size_t) slot] = ItemStack::air();
        owner.dropItem(level, position, item, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
        return true;
    }
    return false;
}

float MobEquipment::absorbDamage(float amount, const ActorDamageSource &source) const {
    return ArmorProtection::apply(&mSlots[HEAD], FEET - HEAD + 1, amount, source.mDeathMessageKey,
                                  source.mArmorEfficiency);
}

void MobEquipment::saveNbt(Tag &data) const {
    data.put(TAG_MAINHAND, singleItemList(mSlots[MAINHAND]));
    data.put(TAG_OFFHAND, singleItemList(mSlots[OFFHAND]));

    std::vector<Tag> armor;
    for (int slot = HEAD; slot <= BODY; ++slot)
        armor.push_back(ItemStackNbt::write(mSlots[(size_t) slot]));
    data.put(TAG_ARMOR, Tag::ofList(Tag::Type::Compound, std::move(armor)));

    if (mInventory.empty())
        return;

    std::vector<Tag> inventory;
    for (size_t slot = 0; slot < mInventory.size(); ++slot)
        inventory.push_back(ItemStackNbt::write(mInventory[slot], (int) slot));
    data.put(TAG_CHEST_ITEMS, Tag::ofList(Tag::Type::Compound, std::move(inventory)));
}

void MobEquipment::loadNbt(const Tag &data, const PacketCodecContext &context) {
    for (ItemStack &item: mSlots)
        item = ItemStack::air();

    const std::vector<Tag> *mainhand = itemList(data, TAG_MAINHAND);
    if (mainhand != nullptr && !mainhand->empty())
        mSlots[MAINHAND] = ItemStackNbt::read(mainhand->front(), context);

    const std::vector<Tag> *offhand = itemList(data, TAG_OFFHAND);
    if (offhand != nullptr && !offhand->empty())
        mSlots[OFFHAND] = ItemStackNbt::read(offhand->front(), context);

    const std::vector<Tag> *body = itemList(data, TAG_BODY);
    if (body != nullptr && !body->empty())
        mSlots[BODY] = ItemStackNbt::read(body->front(), context);

    mInventory.clear();
    const std::vector<Tag> *inventory = itemList(data, TAG_CHEST_ITEMS);
    if (inventory == nullptr)
        inventory = itemList(data, TAG_INVENTORY);
    if (inventory != nullptr) {
        for (const Tag &entry: *inventory) {
            const int slot = ItemStackNbt::readSlot(entry);
            if (slot >= 0)
                setInventoryItem(slot, ItemStackNbt::read(entry, context));
        }
    }

    const std::vector<Tag> *armor = itemList(data, TAG_ARMOR);
    if (armor == nullptr)
        return;

    const size_t count = std::min(armor->size(), (size_t) (BODY - HEAD + 1));
    for (size_t index = 0; index < count; ++index) {
        const ItemStack item = ItemStackNbt::read((*armor)[index], context);
        if (!item.isAir() || HEAD + (int) index != BODY)
            mSlots[(size_t) HEAD + index] = item;
    }
}
