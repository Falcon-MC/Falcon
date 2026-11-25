#include "Protocol/Packets/TrimDataPacket.h"

#include "Protocol/NetworkPacketHandler.h"

TrimDataPacket::TrimDataPacket() = default;

void TrimDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mPatterns.size());
    for (const TrimPattern &pattern: mPatterns) {
        stream.putString(pattern.mItemName);
        stream.putString(pattern.mPatternId);
    }

    stream.putArrayLength((uint32_t) mMaterials.size());
    for (const TrimMaterial &material: mMaterials) {
        stream.putString(material.mMaterialId);
        stream.putString(material.mColor);
        stream.putString(material.mItemName);
    }
}

void TrimDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPatterns.clear();
    uint32_t patternCount = stream.getArrayLength();
    for (uint32_t i = 0; i < patternCount; i++) {
        TrimPattern pattern;
        pattern.mItemName = stream.getString();
        pattern.mPatternId = stream.getString();
        mPatterns.push_back(pattern);
    }

    mMaterials.clear();
    uint32_t materialCount = stream.getArrayLength();
    for (uint32_t i = 0; i < materialCount; i++) {
        TrimMaterial material;
        material.mMaterialId = stream.getString();
        material.mColor = stream.getString();
        material.mItemName = stream.getString();
        mMaterials.push_back(material);
    }
}

void TrimDataPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
