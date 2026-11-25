#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/DataStoreTypes.h"

#include <vector>

class ClientboundDataStorePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundDataStore;

    ClientboundDataStorePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundDataStorePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<DataStoreAction> mUpdates;
};
