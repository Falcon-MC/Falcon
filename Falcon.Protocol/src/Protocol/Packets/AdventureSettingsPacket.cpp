#include "Protocol/Packets/AdventureSettingsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

#include <algorithm>

namespace {

    bool bitForFlags1(AdventureSetting setting, uint32_t &bit) {
        switch (setting) {
            case AdventureSetting::WorldImmutable:
                bit = 1u << 0;
                return true;
            case AdventureSetting::NoPvm:
                bit = 1u << 1;
                return true;
            case AdventureSetting::NoMvp:
                bit = 1u << 2;
                return true;
            case AdventureSetting::ShowNameTags:
                bit = 1u << 4;
                return true;
            case AdventureSetting::AutoJump:
                bit = 1u << 5;
                return true;
            case AdventureSetting::MayFly:
                bit = 1u << 6;
                return true;
            case AdventureSetting::NoClip:
                bit = 1u << 7;
                return true;
            case AdventureSetting::WorldBuilder:
                bit = 1u << 8;
                return true;
            case AdventureSetting::Flying:
                bit = 1u << 9;
                return true;
            case AdventureSetting::Muted:
                bit = 1u << 10;
                return true;
            default:
                return false;
        }
    }

    bool bitForFlags2(AdventureSetting setting, uint32_t &bit) {
        switch (setting) {
            case AdventureSetting::Mine:
                bit = 1u << 0;
                return true;
            case AdventureSetting::DoorsAndSwitches:
                bit = 1u << 1;
                return true;
            case AdventureSetting::OpenContainers:
                bit = 1u << 2;
                return true;
            case AdventureSetting::AttackPlayers:
                bit = 1u << 3;
                return true;
            case AdventureSetting::AttackMobs:
                bit = 1u << 4;
                return true;
            case AdventureSetting::Operator:
                bit = 1u << 5;
                return true;
            case AdventureSetting::Teleport:
                bit = 1u << 7;
                return true;
            case AdventureSetting::Build:
                bit = 1u << 8;
                return true;
            case AdventureSetting::DefaultLevelPermissions:
                bit = 1u << 9;
                return true;
            default:
                return false;
        }
    }

    void readFlags(uint32_t flags, const AdventureSetting *mappings, size_t count, std::vector<AdventureSetting> &settings) {
        for (size_t i = 0; i < count; i++) {
            if (flags & (1u << i)) {
                settings.push_back(mappings[i]);
            }
        }
    }

}

AdventureSettingsPacket::AdventureSettingsPacket() = default;

void AdventureSettingsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    uint32_t flags1 = 0;
    uint32_t flags2 = 0;

    for (AdventureSetting setting : mSettings) {
        uint32_t bit = 0;
        if (bitForFlags1(setting, bit)) {
            flags1 |= bit;
        } else if (bitForFlags2(setting, bit)) {
            flags2 |= bit;
        }
    }

    stream.putUnsignedVarInt(flags1);
    stream.putUnsignedVarInt((uint32_t) mCommandPermission);
    stream.putUnsignedVarInt(flags2);
    stream.putUnsignedVarInt((uint32_t) mPlayerPermission);
    stream.putUnsignedVarInt(0);
    stream.putLLong((uint64_t) mUniqueActorId);
}

void AdventureSettingsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t flags1 = stream.getUnsignedVarInt();
    mCommandPermission = (CommandPermission) stream.getUnsignedVarInt();
    uint32_t flags2 = stream.getUnsignedVarInt();
    mPlayerPermission = (PlayerPermission) stream.getUnsignedVarInt();
    stream.getUnsignedVarInt();
    mUniqueActorId = (int64_t) stream.getLLong();

    static const AdventureSetting FLAGS_1[] = {
            AdventureSetting::WorldImmutable, AdventureSetting::NoPvm, AdventureSetting::NoMvp,
            AdventureSetting::WorldImmutable, AdventureSetting::ShowNameTags, AdventureSetting::AutoJump,
            AdventureSetting::MayFly, AdventureSetting::NoClip, AdventureSetting::WorldBuilder,
            AdventureSetting::Flying, AdventureSetting::Muted,
    };
    static const AdventureSetting FLAGS_2[] = {
            AdventureSetting::Mine, AdventureSetting::DoorsAndSwitches, AdventureSetting::OpenContainers,
            AdventureSetting::AttackPlayers, AdventureSetting::AttackMobs, AdventureSetting::Operator,
            AdventureSetting::Operator, AdventureSetting::Teleport, AdventureSetting::Build,
            AdventureSetting::DefaultLevelPermissions,
    };

    mSettings.clear();

    for (size_t i = 0; i < 11; i++) {
        if (i == 3 || (flags1 & (1u << i)) == 0)
            continue;
        mSettings.push_back(FLAGS_1[i]);
    }

    for (size_t i = 0; i < 10; i++) {
        if (i == 6 || (flags2 & (1u << i)) == 0)
            continue;
        mSettings.push_back(FLAGS_2[i]);
    }
}

void AdventureSettingsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
