#pragma once

#include "Core/Utility/UUID.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/SerializedSkin.h"

#include <cstdint>
#include <string>
#include <vector>

class PlayerListPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerList;

    enum class Action : uint8_t {
        Add = 0,
        Remove = 1,
    };

    class Entry {
    public:
        explicit Entry(const Uuid &uuid) : mUuid(uuid) {}

        Action mAction = Action::Add;
        Uuid mUuid;
        int64_t mActorId = 0;
        std::string mName;
        std::string mXuid;
        std::string mPlayFabId;
        std::string mPlatformChatId;
        int32_t mBuildPlatform = 0;
        SerializedSkin mSkin;
        bool mTeacher = false;
        bool mHost = false;
        bool mSubClient = false;
        int32_t mColorArgb = -1;
    };

    PlayerListPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerListPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<Entry> mEntries;
};
