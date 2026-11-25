#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/DataStoreTypes.h"

class ServerboundDataStorePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerboundDataStore;

    ServerboundDataStorePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerboundDataStorePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    DataStoreUpdate mUpdate;
};
