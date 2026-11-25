#include "Protocol/Packets/SimpleEventPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SimpleEventPacket::SimpleEventPacket()
        : mEvent(0) {}

void SimpleEventPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLShort(mEvent);
}

void SimpleEventPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mEvent = stream.getLShort();
}

void SimpleEventPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
