#include "Protocol/Packets/SpawnExperienceOrbPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SpawnExperienceOrbPacket::SpawnExperienceOrbPacket()
        : mAmount(0) {}

void SpawnExperienceOrbPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVector3f(mPosition);
    stream.putVarInt(mAmount);
}

void SpawnExperienceOrbPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPosition = stream.getVector3f();
    mAmount = stream.getVarInt();
}

void SpawnExperienceOrbPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
