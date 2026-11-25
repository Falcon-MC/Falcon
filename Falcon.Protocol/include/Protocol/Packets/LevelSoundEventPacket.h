#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <string>

namespace LevelSoundEvent {
    inline constexpr const char *BURP = "burp";
    inline constexpr const char *THROW = "throw";
    inline constexpr const char *TELEPORT = "teleport";
    inline constexpr const char *BREAK = "break";
    inline constexpr const char *GLASS = "glass";
    inline constexpr const char *HIT = "hit";
    inline constexpr const char *PLACE = "place";
    inline constexpr const char *ITEM_USE_ON = "item.use.on";
    inline constexpr const char *BUCKET_FILL_WATER = "bucket.fill.water";
    inline constexpr const char *BUCKET_EMPTY_WATER = "bucket.empty.water";
    inline constexpr const char *BUCKET_FILL_LAVA = "bucket.fill.lava";
    inline constexpr const char *BUCKET_EMPTY_LAVA = "bucket.empty.lava";
    inline constexpr const char *BUCKET_FILL_POWDER_SNOW = "bucket.fill.powder_snow";
    inline constexpr const char *BUCKET_EMPTY_POWDER_SNOW = "bucket.empty.powder_snow";
    inline constexpr const char *DOOR_OPEN = "door.open";
    inline constexpr const char *DOOR_CLOSE = "door.close";
    inline constexpr const char *TRAPDOOR_OPEN = "trapdoor.open";
    inline constexpr const char *TRAPDOOR_CLOSE = "trapdoor.close";
    inline constexpr const char *FENCE_GATE_OPEN = "fence_gate.open";
    inline constexpr const char *FENCE_GATE_CLOSE = "fence_gate.close";
    inline constexpr const char *POTION_BREWED = "potion.brewed";
    inline constexpr const char *BEEHIVE_ENTER = "block.beehive.enter";
    inline constexpr const char *BEEHIVE_EXIT = "block.beehive.exit";
    inline constexpr const char *BEEHIVE_WORK = "block.beehive.work";
    inline constexpr const char *BEEHIVE_SHEAR = "block.beehive.shear";
    inline constexpr const char *BOTTLE_FILL = "bottle.fill";
    inline constexpr const char *CREAKING_HEART_SPAWN = "creaking_heart_spawn";
    inline constexpr const char *CREAKING_HEART_TRAIL = "block.creaking_heart.trail";
    inline constexpr const char *SPAWN = "spawn";
    inline constexpr const char *DEATH = "death";
    inline constexpr const char *PLOP = "plop";
    inline constexpr const char *WIND_CHARGE_BURST = "wind_charge.burst";
    inline constexpr const char *DRINK = "drink";
    inline constexpr const char *MACE_SMASH_GROUND = "mace.smash_ground";
    inline constexpr const char *MACE_HEAVY_SMASH_GROUND = "mace.heavy_smash_ground";
    inline constexpr const char *MACE_SMASH_AIR = "mace.smash_air";
    inline constexpr const char *LUNGE_1 = "item.enchant.lunge1";
    inline constexpr const char *LUNGE_2 = "item.enchant.lunge2";
    inline constexpr const char *LUNGE_3 = "item.enchant.lunge3";
    inline constexpr const char *BOW = "bow";
    inline constexpr const char *BOW_HIT = "bow.hit";
    inline constexpr const char *CROSSBOW_LOADING_START = "crossbow.loading.start";
    inline constexpr const char *CROSSBOW_LOADING_MIDDLE = "crossbow.loading.middle";
    inline constexpr const char *CROSSBOW_LOADING_END = "crossbow.loading.end";
    inline constexpr const char *CROSSBOW_QUICK_CHARGE_START = "crossbow.quick_charge.start";
    inline constexpr const char *CROSSBOW_QUICK_CHARGE_MIDDLE = "crossbow.quick_charge.middle";
    inline constexpr const char *CROSSBOW_QUICK_CHARGE_END = "crossbow.quick_charge.end";
    inline constexpr const char *CROSSBOW_SHOOT = "crossbow.shoot";
    inline constexpr const char *TRIDENT_THROW = "item.trident.throw";
    inline constexpr const char *TRIDENT_HIT = "item.trident.hit";
    inline constexpr const char *TRIDENT_HIT_GROUND = "item.trident.hit_ground";
    inline constexpr const char *TRIDENT_RETURN = "item.trident.return";
    inline constexpr const char *TRIDENT_THUNDER = "item.trident.thunder";
    inline constexpr const char *TRIDENT_RIPTIDE_1 = "item.trident.riptide_1";
    inline constexpr const char *TRIDENT_RIPTIDE_2 = "item.trident.riptide_2";
    inline constexpr const char *TRIDENT_RIPTIDE_3 = "item.trident.riptide_3";
    inline constexpr const char *ARMOR_EQUIP_GENERIC = "armor.equip_generic";
    inline constexpr const char *ARMOR_EQUIP_ELYTRA = "armor.equip_elytra";
    inline constexpr const char *PISTON_IN = "piston.in";
    inline constexpr const char *PISTON_OUT = "piston.out";
    inline constexpr const char *CHEST_OPEN = "chest.open";
    inline constexpr const char *CHEST_CLOSED = "chest.closed";
    inline constexpr const char *SHULKER_BOX_OPEN = "shulkerbox.open";
    inline constexpr const char *SHULKER_BOX_CLOSED = "shulkerbox.closed";
    inline constexpr const char *ENDER_CHEST_OPEN = "enderchest.open";
    inline constexpr const char *ENDER_CHEST_CLOSED = "enderchest.closed";
    inline constexpr const char *BARREL_OPEN = "block.barrel.open";
    inline constexpr const char *BARREL_CLOSED = "block.barrel.close";
    inline constexpr const char *LAUNCH = "launch";
    inline constexpr const char *LARGE_BLAST = "large.blast";
    inline constexpr const char *THUNDER = "thunder";
    inline constexpr const char *EXPLODE = "explode";
    inline constexpr const char *BUNDLE_INSERT = "bundle.insert";
    inline constexpr const char *BUNDLE_INSERT_FAIL = "bundle.insert_fail";
    inline constexpr const char *BUNDLE_REMOVE_ONE = "bundle.remove_one";
    inline constexpr const char *BUNDLE_DROP_CONTENTS = "bundle.drop_contents";
    inline constexpr const char *FIRE_IGNITE = "ignite";
    inline constexpr const char *POWER_ON = "power.on";
    inline constexpr const char *POWER_OFF = "power.off";
}

class LevelSoundEventPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::LevelSoundEvent;

    LevelSoundEventPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "LevelSoundEventPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mSound;
    Vector3f mPosition;
    int32_t mExtraData;
    std::string mActorType;
    bool mIsBabyMob;
    bool mDisableRelativeVolume;
    int64_t mActorUniqueId;
    bool mHasFirePosition;
    Vector3f mFirePosition;

    /**
     * Set by the sender when mExtraData is a block network ID, so it is translated for clients of another version.
     * It is not part of the packet.
     */
    bool mExtraDataIsBlock = false;
};
