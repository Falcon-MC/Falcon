#include "Protocol/Packets/ClientCheatAbilityPacket.h"

#include "Protocol/EntityCodec.h"
#include "Protocol/NetworkPacketHandler.h"

ClientCheatAbilityPacket::ClientCheatAbilityPacket() = default;

void ClientCheatAbilityPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    EntityCodec::writePlayerAbilities(stream, mAbilities);
}

void ClientCheatAbilityPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAbilities = EntityCodec::readPlayerAbilities(stream);
}

void ClientCheatAbilityPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
