#include "Protocol/Packets/VoxelShapesPacket.h"

#include "Protocol/NetworkPacketHandler.h"

VoxelShapesPacket::VoxelShapesPacket() = default;

void VoxelShapesPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mShapes.size());
    for (const SerializableVoxelShape &shape: mShapes) {
        stream.putByte(shape.mCells.mXSize);
        stream.putByte(shape.mCells.mYSize);
        stream.putByte(shape.mCells.mZSize);

        stream.putArrayLength((uint32_t) shape.mCells.mStorage.size());
        for (uint8_t value: shape.mCells.mStorage)
            stream.putByte(value);

        stream.putArrayLength((uint32_t) shape.mXCoordinates.size());
        for (float value: shape.mXCoordinates)
            stream.putLFloat(value);

        stream.putArrayLength((uint32_t) shape.mYCoordinates.size());
        for (float value: shape.mYCoordinates)
            stream.putLFloat(value);

        stream.putArrayLength((uint32_t) shape.mZCoordinates.size());
        for (float value: shape.mZCoordinates)
            stream.putLFloat(value);
    }

    stream.putUnsignedVarInt((uint32_t) mNameMap.size());
    for (const std::pair<std::string, uint16_t> &entry: mNameMap) {
        stream.putString(entry.first);
        stream.putLShort(entry.second);
    }

    stream.putLShort(mCustomShapeCount);
}

void VoxelShapesPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mShapes.clear();
    uint32_t shapeCount = stream.getArrayLength();
    for (uint32_t i = 0; i < shapeCount; i++) {
        SerializableVoxelShape shape;
        shape.mCells.mXSize = stream.getByte();
        shape.mCells.mYSize = stream.getByte();
        shape.mCells.mZSize = stream.getByte();

        uint32_t storageCount = stream.getArrayLength();
        for (uint32_t j = 0; j < storageCount; j++)
            shape.mCells.mStorage.push_back(stream.getByte());

        uint32_t xCount = stream.getArrayLength();
        for (uint32_t j = 0; j < xCount; j++)
            shape.mXCoordinates.push_back(stream.getLFloat());

        uint32_t yCount = stream.getArrayLength();
        for (uint32_t j = 0; j < yCount; j++)
            shape.mYCoordinates.push_back(stream.getLFloat());

        uint32_t zCount = stream.getArrayLength();
        for (uint32_t j = 0; j < zCount; j++)
            shape.mZCoordinates.push_back(stream.getLFloat());

        mShapes.push_back(shape);
    }

    mNameMap.clear();
    uint32_t nameCount = stream.getUnsignedVarInt();
    for (uint32_t i = 0; i < nameCount; i++) {
        std::string name = stream.getString();
        uint16_t value = stream.getLShort();
        mNameMap.emplace_back(name, value);
    }

    mCustomShapeCount = stream.getLShort();
}

void VoxelShapesPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
