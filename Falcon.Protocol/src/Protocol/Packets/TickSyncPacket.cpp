#include "Protocol/Packets/TickSyncPacket.h"

#include "Protocol/NetworkPacketHandler.h"

TickSyncPacket::TickSyncPacket() = default;

void TickSyncPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLLong((uint64_t) mRequestTimestamp);
    stream.putLLong((uint64_t) mResponseTimestamp);
}

void TickSyncPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRequestTimestamp = (int64_t) stream.getLLong();
    mResponseTimestamp = (int64_t) stream.getLLong();
}

void TickSyncPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
