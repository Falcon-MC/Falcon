#include "World/Level/Block/Actor/EnderChestBlockActor.h"

#include "World/Level/Block/Inventory/EnderChestInventoryStore.h"

Tag EnderChestBlockActor::saveNbt() const {
    return Tag::ofCompound();
}

void EnderChestBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) data;
    (void) context;
}

Container *EnderChestBlockActor::getContainerFor(int64_t ownerId) {
    return &EnderChestInventoryStore::getInstance().get(ownerId);
}
