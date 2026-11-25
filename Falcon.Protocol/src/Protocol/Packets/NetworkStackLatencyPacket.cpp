#include "Protocol/Packets/NetworkStackLatencyPacket.h"

#include "Protocol/NetworkPacketHandler.h"

NetworkStackLatencyPacket::NetworkStackLatencyPacket()
        : mTimestamp(0), mFromServer(false) {}

void NetworkStackLatencyPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLLong(mTimestamp);
    stream.putBool(mFromServer);
}

void NetworkStackLatencyPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mTimestamp = stream.getLLong();
    mFromServer = stream.getBool();
}

void NetworkStackLatencyPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
