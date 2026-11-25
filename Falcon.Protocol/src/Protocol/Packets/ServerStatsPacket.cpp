#include "Protocol/Packets/ServerStatsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerStatsPacket::ServerStatsPacket()
        : mServerTime(0.0f), mNetworkTime(0.0f) {}

void ServerStatsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLFloat(mServerTime);
    stream.putLFloat(mNetworkTime);
}

void ServerStatsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mServerTime = stream.getLFloat();
    mNetworkTime = stream.getLFloat();
}

void ServerStatsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
