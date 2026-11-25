#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Utility/UUID.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/EntityDataMap.h"
#include "Protocol/Types/EntityLinkData.h"
#include "Protocol/Types/EntityProperties.h"
#include "Protocol/Types/PassengerOfBlockData.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/PlayerAbilityData.h"

#include <cstdint>
#include <string>
#include <vector>

class AddPlayerPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AddPlayer;

    AddPlayerPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AddPlayerPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Uuid mUuid;
    std::string mUsername;
    int64_t mRuntimeActorId = 0;
    std::string mPlatformChatId;
    Vector3f mPosition;
    Vector3f mMotion;
    Vector3f mRotation;
    ItemStack mHand;
    int32_t mGameType = 0;
    EntityDataMap mMetadata;
    EntityProperties mProperties;
    PlayerAbilityData mAbilities;
    std::vector<EntityLinkData> mActorLinks;
    bool mHasPassengerOfBlock = false;
    PassengerOfBlockData mPassengerOfBlock;
    std::string mDeviceId;
    int32_t mBuildPlatform = 0;
};
