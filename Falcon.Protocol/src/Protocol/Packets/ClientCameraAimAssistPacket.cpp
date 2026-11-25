#include "Protocol/Packets/ClientCameraAimAssistPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientCameraAimAssistPacket::ClientCameraAimAssistPacket() = default;

void ClientCameraAimAssistPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mPresetId);
    stream.putByte((unsigned char) mAction);
    stream.putBool(mAllowAimAssist);
}

void ClientCameraAimAssistPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPresetId = stream.getString();
    mAction = (AimAssistAction) stream.getByte();
    mAllowAimAssist = stream.getBool();
}

void ClientCameraAimAssistPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
