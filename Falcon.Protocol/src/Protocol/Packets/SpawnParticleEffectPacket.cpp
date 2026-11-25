#include "Protocol/Packets/SpawnParticleEffectPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SpawnParticleEffectPacket::SpawnParticleEffectPacket()
        : mDimensionId(0), mUniqueActorId(-1), mHasMolangVariablesJson(false) {}

void SpawnParticleEffectPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mDimensionId);
    stream.putVarLong(mUniqueActorId);
    stream.putVector3f(mPosition);
    stream.putString(mIdentifier);

    stream.putOptionalPresent(mHasMolangVariablesJson);
    if (mHasMolangVariablesJson)
        stream.putString(mMolangVariablesJson);
}

void SpawnParticleEffectPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mDimensionId = stream.getByte();
    mUniqueActorId = stream.getVarLong();
    mPosition = stream.getVector3f();
    mIdentifier = stream.getString();

    mHasMolangVariablesJson = stream.getOptionalPresent();
    if (mHasMolangVariablesJson)
        mMolangVariablesJson = stream.getString();
}

void SpawnParticleEffectPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
