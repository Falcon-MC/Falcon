#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/AttributeData.h"
#include "Protocol/Types/EntityDataMap.h"
#include "Protocol/Types/EntityLinkData.h"
#include "Protocol/Types/EntityProperties.h"
#include "Protocol/Types/PassengerOfBlockData.h"

#include <cstdint>
#include <string>
#include <vector>

class AddActorPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AddActor;

    AddActorPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AddActorPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<AttributeData> mAttributes;
    EntityDataMap mMetadata;
    std::vector<EntityLinkData> mActorLinks;
    bool mHasPassengerOfBlock = false;
    PassengerOfBlockData mPassengerOfBlock;
    int64_t mUniqueActorId = 0;
    int64_t mRuntimeActorId = 0;
    std::string mIdentifier;
    int32_t mActorType = 0;
    Vector3f mPosition;
    Vector3f mMotion;
    Vector2f mRotation;
    float mHeadRotation = 0.0f;
    float mBodyRotation = 0.0f;
    EntityProperties mProperties;
};
