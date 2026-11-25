#include "Protocol/Packets/ContainerRegistryCleanupPacket.h"

#include "Protocol/InventoryCodec.h"
#include "Protocol/NetworkPacketHandler.h"

ContainerRegistryCleanupPacket::ContainerRegistryCleanupPacket() = default;

void ContainerRegistryCleanupPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mContainers.size());
    for (const FullContainerName &containerName: mContainers) {
        InventoryCodec::writeFullContainerName(stream, containerName);
    }
}

void ContainerRegistryCleanupPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t length = stream.getArrayLength();
    mContainers.clear();
    mContainers.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
        mContainers.push_back(InventoryCodec::readFullContainerName(stream));
    }
}

void ContainerRegistryCleanupPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
