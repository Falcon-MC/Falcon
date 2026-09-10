#include "Protocol/Packets/RecordStartedPacket.h"

#include "Protocol/NetworkPacketHandler.h"

RecordStartedPacket::RecordStartedPacket() = default;

void RecordStartedPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBlockPosition(mBlockPosition);
    stream.putLLong((uint64_t) mServerSoundHandle);
}

void RecordStartedPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mBlockPosition = stream.getBlockPosition();
    mServerSoundHandle = (int64_t) stream.getLLong();
}

void RecordStartedPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
