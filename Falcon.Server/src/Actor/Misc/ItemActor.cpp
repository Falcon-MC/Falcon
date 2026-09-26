#include "Actor/Misc/ItemActor.h"

#include "Inventory/ItemStackNbt.h"

#include <initializer_list>
#include <utility>

namespace {
    Tag floatList(std::initializer_list<float> values) {
        Tag list = Tag::ofList(Tag::Type::Float);
        for (const float value: values)
            list.addToList(Tag::ofFloat(value));
        return list;
    }

    float listValue(const Tag &data, const char *key, size_t index, float fallback) {
        const Tag *list = data.get(key);
        if (list == nullptr || !list->isList() || list->getList().size() <= index)
            return fallback;
        return list->getList()[index].asFloat();
    }
}

ItemActor::ItemActor(uint64_t runtimeId, const ItemStack &item) : Actor(runtimeId), mItem(item) {
    mFlags.set(ActorFlag::HasCollision, true);
    mFlags.set(ActorFlag::HasGravity, true);
    setMaxHealth((float) MAX_HEALTH);
    setHealth((float) MAX_HEALTH);
}

void ItemActor::tick() {
    if (mPickupDelay > 0)
        mPickupDelay--;

    mAge++;
}

Tag ItemActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putString("identifier", getIdentifier());
    data.putLong("UniqueID", getUniqueId());
    data.put("definitions", Tag::ofList(Tag::Type::End));
    data.put("Pos", floatList({getPosition().x, getPosition().y, getPosition().z}));
    data.put("Rotation", floatList({getRotation().y, getRotation().x}));
    data.put("Motion", floatList({getMotion().x, getMotion().y, getMotion().z}));
    data.putByte("OnGround", isOnGround() ? 1 : 0);
    data.putByte("Invulnerable", 0);
    data.putFloat("FallDistance", 0.0f);
    data.put("Tags", Tag::ofList(Tag::Type::End));

    data.put("Item", ItemStackNbt::write(mItem));
    data.putShort("Age", (int16_t) mAge);
    data.putShort("Health", (int16_t) getHealth());
    data.putLong("OwnerID", -1);
    return data;
}

void ItemActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    if (data.contains("UniqueID"))
        setUniqueId(data.getLong("UniqueID"));

    setPosition(Vector3f(listValue(data, "Pos", 0, 0.0f), listValue(data, "Pos", 1, 0.0f),
                         listValue(data, "Pos", 2, 0.0f)));
    const float yaw = listValue(data, "Rotation", 0, 0.0f);
    setRotation(Vector3f(listValue(data, "Rotation", 1, 0.0f), yaw, yaw));
    setMotion(Vector3f(listValue(data, "Motion", 0, 0.0f), listValue(data, "Motion", 1, 0.0f),
                       listValue(data, "Motion", 2, 0.0f)));

    const Tag *item = data.get("Item");
    if (item != nullptr && item->isCompound())
        mItem = ItemStackNbt::read(*item, context);

    mAge = data.getShort("Age", 0);
    setHealth((float) data.getShort("Health", (int16_t) MAX_HEALTH));
    mPickupDelay = 0;
}
