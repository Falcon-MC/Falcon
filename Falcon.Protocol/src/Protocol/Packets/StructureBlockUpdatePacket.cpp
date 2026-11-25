#include "Protocol/Packets/StructureBlockUpdatePacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    void writeEditorData(BinaryStream &stream, const StructureEditorData &data) {
        stream.putString(data.mName);
        stream.putString(data.mFilteredName);
        stream.putString(data.mDataField);
        stream.putBool(data.mIncludingPlayers);
        stream.putBool(data.mBoundingBoxVisible);
        stream.putVarInt((int32_t) data.mType);
        data.mSettings.write(stream);
        stream.putByte((unsigned char) data.mRedstoneSaveMode);
    }

    StructureEditorData readEditorData(ReadOnlyBinaryStream &stream) {
        StructureEditorData data;
        data.mName = stream.getString();
        data.mFilteredName = stream.getString();
        data.mDataField = stream.getString();
        data.mIncludingPlayers = stream.getBool();
        data.mBoundingBoxVisible = stream.getBool();
        data.mType = (StructureBlockType) stream.getVarInt();
        data.mSettings.read(stream);
        data.mRedstoneSaveMode = (StructureRedstoneSaveMode) stream.getByte();
        return data;
    }

}

StructureBlockUpdatePacket::StructureBlockUpdatePacket() = default;

void StructureBlockUpdatePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBlockPosition(mBlockPosition);
    writeEditorData(stream, mEditorData);
    stream.putBool(mPowered);
    stream.putBool(mWaterlogged);
}

void StructureBlockUpdatePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mBlockPosition = stream.getBlockPosition();
    mEditorData = readEditorData(stream);
    mPowered = stream.getBool();
    mWaterlogged = stream.getBool();
}

void StructureBlockUpdatePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
