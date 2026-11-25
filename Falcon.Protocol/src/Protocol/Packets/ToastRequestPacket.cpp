#include "Protocol/Packets/ToastRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ToastRequestPacket::ToastRequestPacket() {}

void ToastRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mTitle);
    stream.putString(mContent);
}

void ToastRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mTitle = stream.getString();
    mContent = stream.getString();
}

void ToastRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
