#include "World/Actor/Vehicle/ChestVehicleActor.h"

ChestVehicleActor::ChestVehicleActor(uint64_t runtimeId, const std::string &identifier, const ActorSize &size)
        : SizedActor(runtimeId, identifier, size, false), mChest(identifier) {}

bool ChestVehicleActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    return mChest.open(owner, player, *this);
}

void ChestVehicleActor::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t lootingLevel) {
    if (isDead())
        return;
    mChest.dropAll(owner, *this);
    SizedActor::kill(owner, source, lootingLevel);
}

Tag ChestVehicleActor::saveNbt() const {
    Tag data = SizedActor::saveNbt();
    mChest.save(data);
    return data;
}

void ChestVehicleActor::loadEquipment(const Tag &data, const PacketCodecContext &context) {
    mChest.load(data, context);
}
