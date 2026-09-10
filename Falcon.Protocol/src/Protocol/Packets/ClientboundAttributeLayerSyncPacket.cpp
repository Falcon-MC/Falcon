#include "Protocol/Packets/ClientboundAttributeLayerSyncPacket.h"

#include "Protocol/NetworkPacketHandler.h"

#include <array>

namespace {

    const std::array<std::string, 8> BOOL_OPERATIONS = {
        "override", "alpha_blend", "and", "nand", "or", "nor", "xor", "xnor"
    };
    const std::array<std::string, 7> FLOAT_OPERATIONS = {
        "override", "alpha_blend", "add", "subtract", "multiply", "minimum", "maximum"
    };
    const std::array<std::string, 5> COLOR_OPERATIONS = {
        "override", "alpha_blend", "add", "subtract", "multiply"
    };
    const std::array<std::string, 31> CAMERA_EASE_NAMES = {
        "linear", "spring", "in_sine", "out_sine", "in_out_sine", "in_quad", "out_quad", "in_out_quad",
        "in_cubic", "out_cubic", "in_out_cubic", "in_quart", "out_quart", "in_out_quart", "in_quint",
        "out_quint", "in_out_quint", "in_expo", "out_expo", "in_out_expo", "in_circ", "out_circ",
        "in_out_circ", "in_back", "out_back", "in_out_back", "in_elastic", "out_elastic", "in_out_elastic",
        "in_bounce", "out_bounce"
    };

    int32_t indexOf(const std::string &value, const std::string *values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            if (values[i] == value) {
                return (int32_t) i;
            }
        }
        return 0;
    }

    void writeWeight(BinaryStream &stream, const AttributeLayerWeight &weight) {
        if (weight.mType == AttributeLayerWeightType::Float) {
            stream.putUnsignedVarInt(0);
            stream.putLFloat(weight.mFloatValue);
        } else {
            stream.putUnsignedVarInt(1);
            stream.putString(weight.mStringValue);
        }
    }

    AttributeLayerWeight readWeight(ReadOnlyBinaryStream &stream) {
        AttributeLayerWeight weight;
        uint32_t type = stream.getUnsignedVarInt();
        if (type == 0) {
            weight.mType = AttributeLayerWeightType::Float;
            weight.mFloatValue = stream.getLFloat();
        } else {
            weight.mType = AttributeLayerWeightType::String;
            weight.mStringValue = stream.getString();
        }
        return weight;
    }

    void writeSettings(BinaryStream &stream, const AttributeLayerSettings &settings) {
        stream.putLInt((uint32_t) settings.mPriority);
        writeWeight(stream, settings.mWeight);
        stream.putBool(settings.mEnabled);
        stream.putBool(settings.mTransitionsPaused);
    }

    AttributeLayerSettings readSettings(ReadOnlyBinaryStream &stream) {
        AttributeLayerSettings settings;
        settings.mPriority = (int32_t) stream.getLInt();
        settings.mWeight = readWeight(stream);
        settings.mEnabled = stream.getBool();
        settings.mTransitionsPaused = stream.getBool();
        return settings;
    }

    void writeColor255(BinaryStream &stream, const Color255RGBA &color) {
        if (color.mType == Color255Type::String) {
            stream.putUnsignedVarInt(0);
            stream.putString(color.mStringValue);
        } else {
            stream.putUnsignedVarInt(1);
            for (int i = 0; i < 4; i++) {
                stream.putLInt((uint32_t) color.mArrayValue[i]);
            }
        }
    }

    Color255RGBA readColor255(ReadOnlyBinaryStream &stream) {
        Color255RGBA color;
        uint32_t type = stream.getUnsignedVarInt();
        if (type == 0) {
            color.mType = Color255Type::String;
            color.mStringValue = stream.getString();
        } else {
            color.mType = Color255Type::Array;
            for (int i = 0; i < 4; i++) {
                color.mArrayValue[i] = (int32_t) stream.getLInt();
            }
        }
        return color;
    }

    void writeAttributeValue(BinaryStream &stream, const EnvironmentAttributeValue &value) {
        switch (value.mType) {
            case EnvironmentAttributeValueType::Bool:
                stream.putUnsignedVarInt(0);
                stream.putBool(value.mBoolValue);
                stream.putString(BOOL_OPERATIONS[(size_t) value.mBoolOperation]);
                break;
            case EnvironmentAttributeValueType::Float:
                stream.putUnsignedVarInt(1);
                stream.putLFloat(value.mFloatValue);
                stream.putString(FLOAT_OPERATIONS[(size_t) value.mFloatOperation]);
                stream.putOptionalPresent(value.mHasConstraintMin);
                if (value.mHasConstraintMin) {
                    stream.putLFloat(value.mConstraintMin);
                }
                stream.putOptionalPresent(value.mHasConstraintMax);
                if (value.mHasConstraintMax) {
                    stream.putLFloat(value.mConstraintMax);
                }
                break;
            case EnvironmentAttributeValueType::Color:
                stream.putUnsignedVarInt(2);
                writeColor255(stream, value.mColorValue);
                stream.putString(COLOR_OPERATIONS[(size_t) value.mColorOperation]);
                break;
        }
    }

    EnvironmentAttributeValue readAttributeValue(ReadOnlyBinaryStream &stream) {
        EnvironmentAttributeValue value;
        uint32_t type = stream.getUnsignedVarInt();
        switch (type) {
            case 0:
                value.mType = EnvironmentAttributeValueType::Bool;
                value.mBoolValue = stream.getBool();
                value.mBoolOperation = indexOf(stream.getString(), BOOL_OPERATIONS.data(), BOOL_OPERATIONS.size());
                break;
            case 1:
                value.mType = EnvironmentAttributeValueType::Float;
                value.mFloatValue = stream.getLFloat();
                value.mFloatOperation = indexOf(stream.getString(), FLOAT_OPERATIONS.data(), FLOAT_OPERATIONS.size());
                value.mHasConstraintMin = stream.getOptionalPresent();
                if (value.mHasConstraintMin) {
                    value.mConstraintMin = stream.getLFloat();
                }
                value.mHasConstraintMax = stream.getOptionalPresent();
                if (value.mHasConstraintMax) {
                    value.mConstraintMax = stream.getLFloat();
                }
                break;
            case 2:
                value.mType = EnvironmentAttributeValueType::Color;
                value.mColorValue = readColor255(stream);
                value.mColorOperation = indexOf(stream.getString(), COLOR_OPERATIONS.data(), COLOR_OPERATIONS.size());
                break;
        }
        return value;
    }

    void writeEnvironmentAttribute(BinaryStream &stream, const EnvironmentAttributeData &attribute) {
        stream.putString(attribute.mAttributeName);
        stream.putOptionalPresent(attribute.mHasFrom);
        if (attribute.mHasFrom) {
            writeAttributeValue(stream, attribute.mFrom);
        }
        writeAttributeValue(stream, attribute.mAttribute);
        stream.putOptionalPresent(attribute.mHasTo);
        if (attribute.mHasTo) {
            writeAttributeValue(stream, attribute.mTo);
        }
        stream.putLInt((uint32_t) attribute.mCurrentTransitionTicks);
        stream.putLInt((uint32_t) attribute.mTotalTransitionTicks);
        stream.putString(CAMERA_EASE_NAMES[(size_t) attribute.mEasing]);
        stream.putLInt((uint32_t) attribute.mLocalTransitionTicks);
        stream.putBool(attribute.mNoiseTransition);
        stream.putByte((unsigned char) attribute.mNoiseAlignment.mType);
        stream.putUnsignedVarInt((uint32_t) attribute.mNoiseAlignment.mValue);
    }

    EnvironmentAttributeData readEnvironmentAttribute(ReadOnlyBinaryStream &stream) {
        EnvironmentAttributeData attribute;
        attribute.mAttributeName = stream.getString();
        attribute.mHasFrom = stream.getOptionalPresent();
        if (attribute.mHasFrom) {
            attribute.mFrom = readAttributeValue(stream);
        }
        attribute.mAttribute = readAttributeValue(stream);
        attribute.mHasTo = stream.getOptionalPresent();
        if (attribute.mHasTo) {
            attribute.mTo = readAttributeValue(stream);
        }
        attribute.mCurrentTransitionTicks = (int32_t) stream.getLInt();
        attribute.mTotalTransitionTicks = (int32_t) stream.getLInt();
        attribute.mEasing = (CameraEase) indexOf(stream.getString(), CAMERA_EASE_NAMES.data(), CAMERA_EASE_NAMES.size());
        attribute.mLocalTransitionTicks = (int32_t) stream.getLInt();
        attribute.mNoiseTransition = stream.getBool();
        attribute.mNoiseAlignment.mType = (NoiseAlignmentType) stream.getByte();
        attribute.mNoiseAlignment.mValue = (int32_t) stream.getUnsignedVarInt();
        return attribute;
    }

}

ClientboundAttributeLayerSyncPacket::ClientboundAttributeLayerSyncPacket() = default;

void ClientboundAttributeLayerSyncPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt((uint32_t) mData.mType);

    switch (mData.mType) {
        case AttributeLayerSyncPayloadType::UpdateAttributeLayers:
            stream.putArrayLength((uint32_t) mData.mLayers.size());
            for (const AttributeLayerData &layer: mData.mLayers) {
                stream.putString(layer.mLayerName);
                stream.putVarInt(layer.mDimension);
                writeSettings(stream, layer.mSettings);
                stream.putArrayLength((uint32_t) layer.mAttributes.size());
                for (const EnvironmentAttributeData &attribute: layer.mAttributes) {
                    writeEnvironmentAttribute(stream, attribute);
                }
            }
            break;
        case AttributeLayerSyncPayloadType::UpdateAttributeLayerSettings:
            stream.putString(mData.mLayerName);
            stream.putVarInt(mData.mDimension);
            writeSettings(stream, mData.mSettings);
            break;
        case AttributeLayerSyncPayloadType::UpdateEnvironmentAttributes:
            stream.putString(mData.mLayerName);
            stream.putVarInt(mData.mDimension);
            stream.putArrayLength((uint32_t) mData.mAttributes.size());
            for (const EnvironmentAttributeData &attribute: mData.mAttributes) {
                writeEnvironmentAttribute(stream, attribute);
            }
            break;
        case AttributeLayerSyncPayloadType::RemoveEnvironmentAttributes:
            stream.putString(mData.mLayerName);
            stream.putVarInt(mData.mDimension);
            stream.putArrayLength((uint32_t) mData.mRemovedAttributes.size());
            for (const std::string &attribute: mData.mRemovedAttributes) {
                stream.putString(attribute);
            }
            break;
    }
}

void ClientboundAttributeLayerSyncPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mData.mType = (AttributeLayerSyncPayloadType) stream.getUnsignedVarInt();

    switch (mData.mType) {
        case AttributeLayerSyncPayloadType::UpdateAttributeLayers: {
            uint32_t length = stream.getArrayLength();
            mData.mLayers.reserve(length);
            for (uint32_t i = 0; i < length; i++) {
                AttributeLayerData layer;
                layer.mLayerName = stream.getString();
                layer.mDimension = stream.getVarInt();
                layer.mSettings = readSettings(stream);
                uint32_t attributeCount = stream.getArrayLength();
                layer.mAttributes.reserve(attributeCount);
                for (uint32_t j = 0; j < attributeCount; j++) {
                    layer.mAttributes.push_back(readEnvironmentAttribute(stream));
                }
                mData.mLayers.push_back(std::move(layer));
            }
            break;
        }
        case AttributeLayerSyncPayloadType::UpdateAttributeLayerSettings:
            mData.mLayerName = stream.getString();
            mData.mDimension = stream.getVarInt();
            mData.mSettings = readSettings(stream);
            break;
        case AttributeLayerSyncPayloadType::UpdateEnvironmentAttributes: {
            mData.mLayerName = stream.getString();
            mData.mDimension = stream.getVarInt();
            uint32_t attributeCount = stream.getArrayLength();
            mData.mAttributes.reserve(attributeCount);
            for (uint32_t i = 0; i < attributeCount; i++) {
                mData.mAttributes.push_back(readEnvironmentAttribute(stream));
            }
            break;
        }
        case AttributeLayerSyncPayloadType::RemoveEnvironmentAttributes: {
            mData.mLayerName = stream.getString();
            mData.mDimension = stream.getVarInt();
            uint32_t length = stream.getArrayLength();
            mData.mRemovedAttributes.reserve(length);
            for (uint32_t i = 0; i < length; i++) {
                mData.mRemovedAttributes.push_back(stream.getString());
            }
            break;
        }
    }
}

void ClientboundAttributeLayerSyncPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
