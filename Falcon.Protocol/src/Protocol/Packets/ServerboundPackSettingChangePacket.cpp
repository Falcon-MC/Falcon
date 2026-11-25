#include "Protocol/Packets/ServerboundPackSettingChangePacket.h"

#include "Protocol/DataStoreCodec.h"
#include "Protocol/NetworkPacketHandler.h"

ServerboundPackSettingChangePacket::ServerboundPackSettingChangePacket() = default;

void ServerboundPackSettingChangePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUuid(mPackId);
    stream.putString(mPackSettingName);
    DataStoreCodec::writeScalar(stream, mPackSettingValue);
}

void ServerboundPackSettingChangePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPackId = stream.getUuid();
    mPackSettingName = stream.getString();
    mPackSettingValue = DataStoreCodec::readScalar(stream);
}

void ServerboundPackSettingChangePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
