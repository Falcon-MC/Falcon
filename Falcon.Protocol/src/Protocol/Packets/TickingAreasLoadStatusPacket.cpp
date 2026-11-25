#include "Protocol/Packets/TickingAreasLoadStatusPacket.h"

#include "Protocol/NetworkPacketHandler.h"

TickingAreasLoadStatusPacket::TickingAreasLoadStatusPacket()
        : mWaitingForPreload(false) {}

void TickingAreasLoadStatusPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mWaitingForPreload);
}

void TickingAreasLoadStatusPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mWaitingForPreload = stream.getBool();
}

void TickingAreasLoadStatusPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
