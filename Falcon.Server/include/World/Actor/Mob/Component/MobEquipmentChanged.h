#pragma once

#include "World/Actor/Mob/MobEquipment.h"

#include <string>

class MobActor;
class ServerNetworkHandler;

// minecraft:on_equipment_changed: fires on_equip and on_unequip when a listed slot changes, whatever changed it
class MobEquipmentChanged {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

private:
    std::string mItems[MobEquipment::SLOT_COUNT];
    bool mKnown[MobEquipment::SLOT_COUNT] = {};
};
