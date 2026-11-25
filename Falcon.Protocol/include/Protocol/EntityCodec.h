#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/AttributeData.h"
#include "Protocol/Types/EntityDataMap.h"
#include "Protocol/Types/EntityLinkData.h"
#include "Protocol/Types/EntityProperties.h"
#include "Protocol/Types/PassengerOfBlockData.h"
#include "Protocol/Types/PlayerAbilityData.h"

class EntityCodec {
public:
    static void writeAttribute(BinaryStream &stream, const AttributeData &attribute);

    static AttributeData readAttribute(ReadOnlyBinaryStream &stream);

    static void writeEntityData(BinaryStream &stream, const PacketCodecContext &context,
                                const EntityDataMap &entityData);

    static EntityDataMap readEntityData(ReadOnlyBinaryStream &stream);

    static void writeEntityProperties(BinaryStream &stream, const EntityProperties &properties);

    static EntityProperties readEntityProperties(ReadOnlyBinaryStream &stream);

    static void writeEntityLink(BinaryStream &stream, const EntityLinkData &link);

    static EntityLinkData readEntityLink(ReadOnlyBinaryStream &stream);

    static void writePassengerOfBlock(BinaryStream &stream, bool present, const PassengerOfBlockData &data);

    static bool readPassengerOfBlock(ReadOnlyBinaryStream &stream, PassengerOfBlockData &data);

    static void writePlayerAbilities(BinaryStream &stream, const PlayerAbilityData &abilities);

    static PlayerAbilityData readPlayerAbilities(ReadOnlyBinaryStream &stream);
};
