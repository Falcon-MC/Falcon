#include "Protocol/Packets/EditorNetworkPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

EditorNetworkPacket::EditorNetworkPacket() = default;

void EditorNetworkPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mRouteToManager);
    NbtIo::writeTag(stream, mPayload, NbtVariant::Network);
}

void EditorNetworkPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRouteToManager = stream.getBool();
    mPayload = NbtIo::readTag(stream, NbtVariant::Network);
}

void EditorNetworkPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
