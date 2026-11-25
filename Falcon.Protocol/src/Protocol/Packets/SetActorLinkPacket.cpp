#include "Protocol/Packets/SetActorLinkPacket.h"

#include "Protocol/EntityCodec.h"
#include "Protocol/NetworkPacketHandler.h"

SetActorLinkPacket::SetActorLinkPacket() = default;

void SetActorLinkPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    EntityCodec::writeEntityLink(stream, mActorLink);
}

void SetActorLinkPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mActorLink = EntityCodec::readEntityLink(stream);
}

void SetActorLinkPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
