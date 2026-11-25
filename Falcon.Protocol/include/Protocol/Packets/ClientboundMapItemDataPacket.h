#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/MapTypes.h"

#include <cstdint>
#include <vector>

class ClientboundMapItemDataPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundMapItemData;

    ClientboundMapItemDataPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundMapItemDataPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueMapId = 0;
    int32_t mDimensionId = 0;
    bool mLocked = false;
    Vector3i mOrigin;

    bool mHasTrackedActorIds = false;
    std::vector<int64_t> mTrackedActorIds;

    bool mHasScale = false;
    int8_t mScale = 0;

    bool mHasTrackedObjects = false;
    std::vector<MapTrackedObject> mTrackedObjects;

    bool mHasDecorations = false;
    std::vector<MapDecoration> mDecorations;

    bool mHasWidth = false;
    int32_t mWidth = 0;

    bool mHasHeight = false;
    int32_t mHeight = 0;

    bool mHasXOffset = false;
    int32_t mXOffset = 0;

    bool mHasYOffset = false;
    int32_t mYOffset = 0;

    bool mHasColors = false;
    std::vector<int32_t> mColors;
};
