#include "Protocol/Packets/ServerboundDataStorePacket.h"

#include "Protocol/DataStoreCodec.h"
#include "Protocol/NetworkPacketHandler.h"

ServerboundDataStorePacket::ServerboundDataStorePacket() = default;

void ServerboundDataStorePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mUpdate.mDataStoreName);
    stream.putString(mUpdate.mProperty);
    stream.putString(mUpdate.mPath);
    DataStoreCodec::writeScalar(stream, mUpdate.mData);
    stream.putLInt((uint32_t) mUpdate.mUpdateCount);
    stream.putLInt((uint32_t) mUpdate.mPathUpdateCount);
}

void ServerboundDataStorePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUpdate.mDataStoreName = stream.getString();
    mUpdate.mProperty = stream.getString();
    mUpdate.mPath = stream.getString();
    mUpdate.mData = DataStoreCodec::readScalar(stream);
    mUpdate.mUpdateCount = (int32_t) stream.getLInt();
    mUpdate.mPathUpdateCount = (int32_t) stream.getLInt();
}

void ServerboundDataStorePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
