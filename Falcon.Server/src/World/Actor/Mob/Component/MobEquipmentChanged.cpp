#include "World/Actor/Mob/Component/MobEquipmentChanged.h"

#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Mob/MobActor.h"
#include "Protocol/Types/ItemDefinition.h"

namespace {
    const char *const ON_EQUIPMENT_CHANGED_COMPONENT = "minecraft:on_equipment_changed";

    std::string identifierOf(const ItemStack &item) {
        return item.isAir() || item.mDefinition == nullptr ? std::string()
                                                           : std::string(item.mDefinition->getIdentifier());
    }

    void fire(ServerNetworkHandler &owner, MobActor &mob, const json::Value &slot, const char *key) {
        const json::Value *event = slot.get(key);
        if (event != nullptr && event->isString())
            mob.fireEvent(owner, event->mString);
        else
            EntityEvents::fireTrigger(owner, mob, event);
    }
}

void MobEquipmentChanged::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(ON_EQUIPMENT_CHANGED_COMPONENT);
    const json::Value *slots = component == nullptr ? nullptr : component->get("slots");
    if (slots == nullptr || !slots->isArray())
        return;

    for (const std::unique_ptr<json::Value> &slot: slots->mArray) {
        const json::Value *name = slot->get("slot");
        const int index = name == nullptr ? -1 : MobEquipment::slotIndexOf(name->string());
        if (index < 0 || index >= MobEquipment::SLOT_COUNT)
            continue;

        const std::string current = identifierOf(mob.getEquipment().getSlot(index));
        // the first look only learns the slot: a loaded actor already has the groups its item gave it
        if (!mKnown[index]) {
            mKnown[index] = true;
            mItems[index] = current;
            continue;
        }
        if (current == mItems[index])
            continue;

        const bool hadItem = !mItems[index].empty();
        mItems[index] = current;
        if (hadItem)
            fire(owner, mob, *slot, "on_unequip");
        if (!current.empty())
            fire(owner, mob, *slot, "on_equip");
    }
}
