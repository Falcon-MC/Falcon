#include "Protocol/Packets/NetworkSettingsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

NetworkSettingsPacket::NetworkSettingsPacket()
        : mCompressionThreshold(0), mCompressionAlgorithm(CompressionAlgorithm::ZLib), mClientThrottleEnabled(false), mClientThrottleThreshold(0), mClientThrottleScalar(0.0f) {
    mCompressible = Packet::Compressibility::Incompressible;
}

void NetworkSettingsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLShort(mCompressionThreshold);
    stream.putLShort((uint16_t) mCompressionAlgorithm);
    stream.putBool(mClientThrottleEnabled);
    stream.putByte((unsigned char) mClientThrottleThreshold);
    stream.putLFloat(mClientThrottleScalar);
}

void NetworkSettingsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCompressionThreshold = stream.getLShort();
    mCompressionAlgorithm = (CompressionAlgorithm) stream.getLShort();
    mClientThrottleEnabled = stream.getBool();
    mClientThrottleThreshold = stream.getByte();
    mClientThrottleScalar = stream.getLFloat();
}

void NetworkSettingsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
