#include "Protocol/Packets/PlayStatusPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlayStatusPacket::PlayStatusPacket()
        : mStatus(Status::LoginSuccess) {}

void PlayStatusPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putInt((uint32_t) mStatus);
}

void PlayStatusPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mStatus = (Status) stream.getSignedInt();
}

void PlayStatusPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
