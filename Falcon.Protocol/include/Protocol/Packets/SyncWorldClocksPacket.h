#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/SyncWorldClocksData.h"

#include <cstdint>
#include <vector>

enum class SyncWorldClocksType : uint32_t {
    SyncState = 0,
    InitializeRegistry = 1,
    AddTimeMarker = 2,
    RemoveTimeMarker = 3
};

class SyncWorldClocksPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SyncWorldClocks;

    SyncWorldClocksPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SyncWorldClocksPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    SyncWorldClocksType mType = SyncWorldClocksType::SyncState;

    // SyncState
    std::vector<SyncWorldClockStateData> mClockStates;

    // InitializeRegistry
    std::vector<WorldClockData> mClocks;

    // AddTimeMarker
    uint64_t mAddClockId = 0;
    std::vector<TimeMarkerData> mAddTimeMarkers;

    // RemoveTimeMarker
    uint64_t mRemoveClockId = 0;
    std::vector<uint64_t> mRemoveTimeMarkerIds;
};
