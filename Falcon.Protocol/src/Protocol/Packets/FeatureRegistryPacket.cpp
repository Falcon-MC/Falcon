#include "Protocol/Packets/FeatureRegistryPacket.h"

#include "Protocol/NetworkPacketHandler.h"

FeatureRegistryPacket::FeatureRegistryPacket() = default;

void FeatureRegistryPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mFeatures.size());
    for (const FeatureDefinition &feature: mFeatures) {
        stream.putString(feature.mName);
        stream.putString(feature.mJson);
    }
}

void FeatureRegistryPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mFeatures.clear();
    uint32_t count = stream.getArrayLength();
    for (uint32_t i = 0; i < count; i++) {
        FeatureDefinition feature;
        feature.mName = stream.getString();
        feature.mJson = stream.getString();
        mFeatures.push_back(feature);
    }
}

void FeatureRegistryPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
