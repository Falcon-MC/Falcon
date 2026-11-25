#include "Protocol/EntityCodec.h"

#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryDataException.h"

void EntityCodec::writeAttribute(BinaryStream &stream, const AttributeData &attribute) {
    stream.putLFloat(attribute.mMinimum);
    stream.putLFloat(attribute.mMaximum);
    stream.putLFloat(attribute.mValue);
    stream.putLFloat(attribute.mDefaultMinimum);
    stream.putLFloat(attribute.mDefaultMaximum);
    stream.putLFloat(attribute.mDefaultValue);
    stream.putString(attribute.mName);

    stream.putArrayLength((uint32_t) attribute.mModifiers.size());
    for (const AttributeModifierData &modifier: attribute.mModifiers) {
        stream.putString(modifier.mId);
        stream.putString(modifier.mName);
        stream.putLFloat(modifier.mAmount);
        stream.putLInt((uint32_t) modifier.mOperation);
        stream.putLInt((uint32_t) modifier.mOperand);
        stream.putBool(modifier.mSerializable);
    }
}

AttributeData EntityCodec::readAttribute(ReadOnlyBinaryStream &stream) {
    AttributeData attribute;
    attribute.mMinimum = stream.getLFloat();
    attribute.mMaximum = stream.getLFloat();
    attribute.mValue = stream.getLFloat();
    attribute.mDefaultMinimum = stream.getLFloat();
    attribute.mDefaultMaximum = stream.getLFloat();
    attribute.mDefaultValue = stream.getLFloat();
    attribute.mName = stream.getString();

    uint32_t modifierCount = stream.getArrayLength();
    attribute.mModifiers.reserve(modifierCount);
    for (uint32_t i = 0; i < modifierCount; i++) {
        AttributeModifierData modifier;
        modifier.mId = stream.getString();
        modifier.mName = stream.getString();
        modifier.mAmount = stream.getLFloat();
        modifier.mOperation = (AttributeOperation) stream.getLInt();
        modifier.mOperand = (int32_t) stream.getLInt();
        modifier.mSerializable = stream.getBool();
        attribute.mModifiers.push_back(std::move(modifier));
    }

    return attribute;
}

void EntityCodec::writeEntityData(BinaryStream &stream, const PacketCodecContext &context,
                                  const EntityDataMap &entityData) {
    stream.putUnsignedVarInt((uint32_t) entityData.mEntries.size());

    for (const EntityDataEntry &entry: entityData.mEntries) {
        stream.putUnsignedVarInt((uint32_t) entry.mId);
        stream.putUnsignedVarInt((uint32_t) entry.mFormat);
        stream.putByte((unsigned char) entry.mFormat);

        switch (entry.mFormat) {
            case EntityDataFormat::Byte:
                stream.putByte((unsigned char) entry.mByteValue);
                break;
            case EntityDataFormat::Short:
                stream.putLShort((uint16_t) entry.mShortValue);
                break;
            case EntityDataFormat::Int:
                stream.putVarInt(entry.mIsBlock ? context.toNetworkBlockId(entry.mIntValue) : entry.mIntValue);
                break;
            case EntityDataFormat::Float:
                stream.putLFloat(entry.mFloatValue);
                break;
            case EntityDataFormat::String:
                stream.putString(entry.mStringValue);
                break;
            case EntityDataFormat::Nbt:
                NbtIo::writeTag(stream, entry.mNbtValue, NbtVariant::Network);
                break;
            case EntityDataFormat::Vector3i:
                stream.putVector3i(entry.mVector3iValue);
                break;
            case EntityDataFormat::Long:
                stream.putVarLong(entry.mLongValue);
                break;
            case EntityDataFormat::Vector3f:
                stream.putVector3f(entry.mVector3fValue);
                break;
        }
    }
}

EntityDataMap EntityCodec::readEntityData(ReadOnlyBinaryStream &stream) {
    EntityDataMap map;

    uint32_t length = stream.getArrayLength();
    map.mEntries.reserve(length);

    for (uint32_t i = 0; i < length; i++) {
        EntityDataEntry entry;
        entry.mId = (int32_t) stream.getUnsignedVarInt();
        entry.mFormat = (EntityDataFormat) stream.getUnsignedVarInt();
        stream.getByte();

        switch (entry.mFormat) {
            case EntityDataFormat::Byte:
                entry.mByteValue = (int8_t) stream.getByte();
                break;
            case EntityDataFormat::Short:
                entry.mShortValue = stream.getSignedLShort();
                break;
            case EntityDataFormat::Int:
                entry.mIntValue = stream.getVarInt();
                break;
            case EntityDataFormat::Float:
                entry.mFloatValue = stream.getLFloat();
                break;
            case EntityDataFormat::String:
                entry.mStringValue = stream.getString();
                break;
            case EntityDataFormat::Nbt:
                entry.mNbtValue = NbtIo::readTag(stream, NbtVariant::Network);
                break;
            case EntityDataFormat::Vector3i:
                entry.mVector3iValue = stream.getVector3i();
                break;
            case EntityDataFormat::Long:
                entry.mLongValue = stream.getVarLong();
                break;
            case EntityDataFormat::Vector3f:
                entry.mVector3fValue = stream.getVector3f();
                break;
            default:
                throw BinaryDataException("Unknown entity data format " + std::to_string((int) entry.mFormat));
        }

        map.mEntries.push_back(entry);
    }

    return map;
}

void EntityCodec::writeEntityProperties(BinaryStream &stream, const EntityProperties &properties) {
    stream.putArrayLength((uint32_t) properties.mIntProperties.size());
    for (const IntEntityProperty &property: properties.mIntProperties) {
        stream.putUnsignedVarInt((uint32_t) property.mIndex);
        stream.putVarInt(property.mValue);
    }

    stream.putArrayLength((uint32_t) properties.mFloatProperties.size());
    for (const FloatEntityProperty &property: properties.mFloatProperties) {
        stream.putUnsignedVarInt((uint32_t) property.mIndex);
        stream.putLFloat(property.mValue);
    }
}

EntityProperties EntityCodec::readEntityProperties(ReadOnlyBinaryStream &stream) {
    EntityProperties properties;

    uint32_t intCount = stream.getArrayLength();
    properties.mIntProperties.reserve(intCount);
    for (uint32_t i = 0; i < intCount; i++) {
        IntEntityProperty property;
        property.mIndex = (int32_t) stream.getUnsignedVarInt();
        property.mValue = stream.getVarInt();
        properties.mIntProperties.push_back(property);
    }

    uint32_t floatCount = stream.getArrayLength();
    properties.mFloatProperties.reserve(floatCount);
    for (uint32_t i = 0; i < floatCount; i++) {
        FloatEntityProperty property;
        property.mIndex = (int32_t) stream.getUnsignedVarInt();
        property.mValue = stream.getLFloat();
        properties.mFloatProperties.push_back(property);
    }

    return properties;
}

void EntityCodec::writeEntityLink(BinaryStream &stream, const EntityLinkData &link) {
    stream.putVarLong(link.mFrom);
    stream.putVarLong(link.mTo);
    stream.putByte((unsigned char) link.mType);
    stream.putBool(link.mImmediate);
    stream.putBool(link.mRiderInitiated);
    stream.putLFloat(link.mVehicleAngularVelocity);
}

EntityLinkData EntityCodec::readEntityLink(ReadOnlyBinaryStream &stream) {
    EntityLinkData link;
    link.mFrom = stream.getVarLong();
    link.mTo = stream.getVarLong();
    link.mType = (EntityLinkType) stream.getByte();
    link.mImmediate = stream.getBool();
    link.mRiderInitiated = stream.getBool();
    link.mVehicleAngularVelocity = stream.getLFloat();
    return link;
}

void EntityCodec::writePassengerOfBlock(BinaryStream &stream, bool present, const PassengerOfBlockData &data) {
    stream.putOptionalPresent(present);
    if (!present)
        return;

    stream.putBlockPosition(data.mBlockPosition);
    stream.putVector3f(data.mOffset);
    stream.putLFloat(data.mRotation);
    stream.putLFloat(data.mRotationLimit);
    stream.putByte((unsigned char) data.mEmoteType);
}

bool EntityCodec::readPassengerOfBlock(ReadOnlyBinaryStream &stream, PassengerOfBlockData &data) {
    if (!stream.getOptionalPresent())
        return false;

    data.mBlockPosition = stream.getBlockPosition();
    data.mOffset = stream.getVector3f();
    data.mRotation = stream.getLFloat();
    data.mRotationLimit = stream.getLFloat();
    data.mEmoteType = (PassengerOfBlockEmoteType) stream.getByte();
    return true;
}

void EntityCodec::writePlayerAbilities(BinaryStream &stream, const PlayerAbilityData &abilities) {
    stream.putLLong((uint64_t) abilities.mUniqueActorId);
    stream.putByte(abilities.mPlayerPermission);
    stream.putByte(abilities.mCommandPermission);

    stream.putArrayLength((uint32_t) abilities.mAbilityLayers.size());
    for (const AbilityLayer &layer: abilities.mAbilityLayers) {
        stream.putLShort(layer.mLayerType);
        stream.putLInt(layer.mAbilitiesSet);
        stream.putLInt(layer.mAbilityValues);
        stream.putLFloat(layer.mFlySpeed);
        stream.putLFloat(layer.mVerticalFlySpeed);
        stream.putLFloat(layer.mWalkSpeed);
    }
}

PlayerAbilityData EntityCodec::readPlayerAbilities(ReadOnlyBinaryStream &stream) {
    PlayerAbilityData abilities;
    abilities.mUniqueActorId = (int64_t) stream.getLLong();
    abilities.mPlayerPermission = stream.getByte();
    abilities.mCommandPermission = stream.getByte();

    uint32_t layerCount = stream.getArrayLength();
    abilities.mAbilityLayers.reserve(layerCount);
    for (uint32_t i = 0; i < layerCount; i++) {
        AbilityLayer layer;
        layer.mLayerType = stream.getLShort();
        layer.mAbilitiesSet = stream.getLInt();
        layer.mAbilityValues = stream.getLInt();
        layer.mFlySpeed = stream.getLFloat();
        layer.mVerticalFlySpeed = stream.getLFloat();
        layer.mWalkSpeed = stream.getLFloat();
        abilities.mAbilityLayers.push_back(layer);
    }

    return abilities;
}
