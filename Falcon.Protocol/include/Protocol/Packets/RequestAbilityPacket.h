#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

enum class Ability : int32_t {
    Build = 0,
    Mine = 1,
    DoorsAndSwitches = 2,
    OpenContainers = 3,
    AttackPlayers = 4,
    AttackMobs = 5,
    OperatorCommands = 6,
    Teleport = 7,
    Invulnerable = 8,
    Flying = 9,
    MayFly = 10,
    Instabuild = 11,
    Lightning = 12,
    FlySpeed = 13,
    WalkSpeed = 14,
    Muted = 15,
    WorldBuilder = 16,
    NoClip = 17,
    PrivilegedBuilder = 18,
    VerticalFlySpeed = 19
};

enum class AbilityValueType : unsigned char {
    None = 0,
    Boolean = 1,
    Float = 2
};

class RequestAbilityPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::RequestAbility;

    RequestAbilityPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "RequestAbilityPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Ability mAbility = Ability::Build;
    AbilityValueType mType = AbilityValueType::None;
    bool mBoolValue = false;
    float mFloatValue = 0.0f;
};
