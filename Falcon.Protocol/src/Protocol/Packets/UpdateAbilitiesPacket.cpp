#include "Protocol/Packets/UpdateAbilitiesPacket.h"

#include "Protocol/EntityCodec.h"
#include "Protocol/NetworkPacketHandler.h"

UpdateAbilitiesPacket::UpdateAbilitiesPacket() = default;

void UpdateAbilitiesPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    EntityCodec::writePlayerAbilities(stream, mAbilities);
}

void UpdateAbilitiesPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAbilities = EntityCodec::readPlayerAbilities(stream);
}

void UpdateAbilitiesPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
