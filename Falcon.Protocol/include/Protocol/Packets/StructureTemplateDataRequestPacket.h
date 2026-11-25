#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/StructureSettings.h"

#include <string>

class StructureTemplateDataRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::StructureTemplateDataRequest;

    enum class Operation : int {
        None = 0,
        ExportFromSavedMode = 1,
        ExportFromLoadMode = 2,
        QuerySavedStructure = 3,
        Import = 4
    };

    StructureTemplateDataRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "StructureTemplateDataRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mName;
    Vector3i mPosition;
    StructureSettings mSettings;
    Operation mOperation;
};
