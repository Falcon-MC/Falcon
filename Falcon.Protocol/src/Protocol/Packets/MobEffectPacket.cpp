#include "Protocol/Packets/MobEffectPacket.h"

#include "Protocol/NetworkPacketHandler.h"

MobEffectPacket::MobEffectPacket()
        : mRuntimeActorId(0), mEvent(Event::None), mEffectId(0), mAmplifier(0), mParticles(false), mDuration(0), mTick(0), mAmbient(false) {}

void MobEffectPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putByte((unsigned char) mEvent);
    stream.putVarInt(mEffectId);
    stream.putVarInt(mAmplifier);
    stream.putBool(mParticles);
    stream.putVarInt(mDuration);
    stream.putUnsignedVarLong(mTick);
    stream.putBool(mAmbient);
}

void MobEffectPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getUnsignedVarLong();
    mEvent = (Event) stream.getByte();
    mEffectId = stream.getVarInt();
    mAmplifier = stream.getVarInt();
    mParticles = stream.getBool();
    mDuration = stream.getVarInt();
    mTick = stream.getUnsignedVarLong();
    mAmbient = stream.getBool();
}

void MobEffectPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
