#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/NBT/Tag.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/ItemStack.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class Actor;
class ActorDamageSource;
class Level;
class MobActor;
class ServerNetworkHandler;
class ServerPlayer;

class MobEquipment {
public:
    static const int MAINHAND = 0;
    static const int OFFHAND = 1;
    static const int HEAD = 2;
    static const int CHEST = 3;
    static const int LEGS = 4;
    static const int FEET = 5;
    static const int BODY = 6;
    static const int SLOT_COUNT = 7;

    static int slotIndexOf(const std::string &slotName);

    const ItemStack &getSlot(int slot) const;

    void setSlot(int slot, ItemStack item);

    const ItemStack &getInventoryItem(int slot) const;

    void setInventoryItem(int slot, ItemStack item);

    int getInventorySize() const {
        return (int) mInventory.size();
    }

    void ensureInventorySize(int size);

    void equipFromTable(ServerNetworkHandler &owner, const MobActor &mob);

    void sendTo(ServerNetworkHandler &owner, const ServerPlayer &player, const Actor &actor) const;

    void broadcast(ServerNetworkHandler &owner, const Actor &actor) const;

    void dropOnDeath(ServerNetworkHandler &owner, Level &level, const MobActor &mob, bool killedByPlayer,
                     int32_t lootingLevel);

    void dropAll(ServerNetworkHandler &owner, Level &level, const Vector3f &position);

    void dropInventory(ServerNetworkHandler &owner, Level &level, const Vector3f &position);

    bool addInventoryItem(const ItemStack &item, int capacity);

    bool dropSlot(ServerNetworkHandler &owner, Level &level, const Vector3f &position, const std::string &slotName);

    float absorbDamage(float amount, const ActorDamageSource &source) const;

    void saveNbt(Tag &data) const;

    void loadNbt(const Tag &data, const PacketCodecContext &context);

private:
    bool _isEmpty() const;

    int _slotFor(const ItemStack &item) const;

    static float _dropChance(const MobActor &mob, int slot);

    static void _damageForDrop(ItemStack &item);

    std::array<ItemStack, SLOT_COUNT> mSlots;
    std::vector<ItemStack> mInventory;
};
