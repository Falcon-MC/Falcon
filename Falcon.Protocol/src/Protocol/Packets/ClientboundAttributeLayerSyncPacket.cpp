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

    const std::array<std::string, 31> CAMERA_EASE_NAMES = {
        "linear", "spring", "in_sine", "out_sine", "in_out_sine", "in_quad", "out_quad", "in_out_quad",
        "in_cubic", "out_cubic", "in_out_cubic", "in_quart", "out_quart", "in_out_quart", "in_quint",
        "out_quint", "in_out_quint", "in_expo", "out_expo", "in_out_expo", "in_circ", "out_circ",
        "in_out_circ", "in_back", "out_back", "in_out_back", "in_elastic", "out_elastic", "in_out_elastic",
        "in_bounce", "out_bounce"
    };

    /**
     * Before payload types, an attribute was sent as a value with optional start and end values and one set of
     * transition fields, and a transition was a noise transition when its flag said so.
     */
    void writeLegacyEnvironmentAttribute(BinaryStream &stream, const EnvironmentAttributeData &attribute,
                                         bool noiseAlignment) {
        const bool transition = attribute.mPayloadType != EnvironmentAttributePayloadType::Constant;
        const bool noise = attribute.mPayloadType == EnvironmentAttributePayloadType::NoiseTransition;
        const AttributeNoiseTransitionSettings &noiseSettings = attribute.mNoiseTransitionSettings;
        const AttributeTransitionSettings &settings = attribute.mTransitionSettings;

        stream.putString(attribute.mAttributeName);
        stream.putOptionalPresent(transition);
        if (transition)
            writeAttributeValue(stream, attribute.mFrom);
        writeAttributeValue(stream, transition ? attribute.mTo : attribute.mAttribute);
        stream.putOptionalPresent(transition);
        if (transition)
            writeAttributeValue(stream, attribute.mTo);
        stream.putLInt(noise ? noiseSettings.mCurrentTransitionTicks : settings.mCurrentTransitionTicks);
        stream.putLInt(noise ? noiseSettings.mTotalTransitionTicks : settings.mTotalTransitionTicks);
        stream.putString(CAMERA_EASE_NAMES[(size_t) (noise ? noiseSettings.mEasing : settings.mEasing)]);
        stream.putLInt(noise ? noiseSettings.mLocalTransitionTicks : 0);
        stream.putBool(noise);
        if (!noiseAlignment)
            return;
        stream.putByte((unsigned char) noiseSettings.mNoiseAlignment.mType);
        stream.putUnsignedVarInt((uint32_t) noiseSettings.mNoiseAlignment.mValue);
    }

    EnvironmentAttributeData readLegacyEnvironmentAttribute(ReadOnlyBinaryStream &stream, bool noiseAlignment) {
        EnvironmentAttributeData attribute;
        attribute.mAttributeName = stream.getString();
        const bool hasFrom = stream.getOptionalPresent();
        if (hasFrom)
            attribute.mFrom = readAttributeValue(stream);
        attribute.mAttribute = readAttributeValue(stream);
        const bool hasTo = stream.getOptionalPresent();
        if (hasTo)
            attribute.mTo = readAttributeValue(stream);

        const uint32_t currentTicks = stream.getLInt();
        const uint32_t totalTicks = stream.getLInt();
        const CameraEase easing =
                (CameraEase) indexOf(stream.getString(), CAMERA_EASE_NAMES.data(), CAMERA_EASE_NAMES.size());
        const uint32_t localTicks = stream.getLInt();
        const bool noise = stream.getBool();
        NoiseAlignment alignment;
        if (noiseAlignment) {
            alignment.mType = (NoiseAlignmentType) stream.getByte();
            alignment.mValue = (int32_t) stream.getUnsignedVarInt();
        }

        if (!hasFrom && !hasTo)
            return attribute;
        if (!noise) {
            attribute.mPayloadType = EnvironmentAttributePayloadType::Transition;
            attribute.mTransitionSettings.mCurrentTransitionTicks = currentTicks;
            attribute.mTransitionSettings.mTotalTransitionTicks = totalTicks;
            attribute.mTransitionSettings.mEasing = easing;
            return attribute;
        }

        attribute.mPayloadType = EnvironmentAttributePayloadType::NoiseTransition;
        attribute.mNoiseTransitionSettings.mCurrentTransitionTicks = currentTicks;
        attribute.mNoiseTransitionSettings.mTotalTransitionTicks = totalTicks;
        attribute.mNoiseTransitionSettings.mEasing = easing;
        attribute.mNoiseTransitionSettings.mLocalTransitionTicks = localTicks;
        attribute.mNoiseTransitionSettings.mNoiseAlignment = alignment;
        return attribute;
    }

    void writePayloadEnvironmentAttribute(BinaryStream &stream, const EnvironmentAttributeData &attribute) {
        stream.putString(attribute.mAttributeName);
        stream.putUnsignedVarInt((uint32_t) attribute.mPayloadType);
        switch (attribute.mPayloadType) {
            case EnvironmentAttributePayloadType::Constant:
                writeAttributeValue(stream, attribute.mAttribute);
                break;
            case EnvironmentAttributePayloadType::Transition: {
                const AttributeTransitionSettings &settings = attribute.mTransitionSettings;
                writeAttributeValue(stream, attribute.mFrom);
                writeAttributeValue(stream, attribute.mTo);
                stream.putUnsignedVarInt(settings.mTotalTransitionTicks);
                stream.putUnsignedVarInt(settings.mCurrentTransitionTicks);
                stream.putVarInt((int32_t) settings.mEasing);
                stream.putString(settings.mClockName);
                break;
            }
            case EnvironmentAttributePayloadType::NoiseTransition: {
                const AttributeNoiseTransitionSettings &settings = attribute.mNoiseTransitionSettings;
                writeAttributeValue(stream, attribute.mFrom);
                writeAttributeValue(stream, attribute.mTo);
                stream.putUnsignedVarInt(settings.mTotalTransitionTicks);
                stream.putUnsignedVarInt(settings.mCurrentTransitionTicks);
                stream.putVarInt((int32_t) settings.mEasing);
                stream.putString(settings.mClockName);
                stream.putUnsignedVarInt(settings.mLocalTransitionTicks);
                stream.putString(settings.mNoiseName);
                stream.putByte((unsigned char) settings.mNoiseAlignment.mType);
                stream.putUnsignedVarInt((uint32_t) settings.mNoiseAlignment.mValue);
                break;
            }
        }
    }

    EnvironmentAttributeData readPayloadEnvironmentAttribute(ReadOnlyBinaryStream &stream) {
        EnvironmentAttributeData attribute;
        attribute.mAttributeName = stream.getString();
        attribute.mPayloadType = (EnvironmentAttributePayloadType) stream.getUnsignedVarInt();
        switch (attribute.mPayloadType) {
            case EnvironmentAttributePayloadType::Constant:
                attribute.mAttribute = readAttributeValue(stream);
                break;
            case EnvironmentAttributePayloadType::Transition: {
                AttributeTransitionSettings &settings = attribute.mTransitionSettings;
                attribute.mFrom = readAttributeValue(stream);
                attribute.mTo = readAttributeValue(stream);
                settings.mTotalTransitionTicks = stream.getUnsignedVarInt();
                settings.mCurrentTransitionTicks = stream.getUnsignedVarInt();
                settings.mEasing = (CameraEase) stream.getVarInt();
                settings.mClockName = stream.getString();
                break;
            }
            case EnvironmentAttributePayloadType::NoiseTransition: {
                AttributeNoiseTransitionSettings &settings = attribute.mNoiseTransitionSettings;
                attribute.mFrom = readAttributeValue(stream);
                attribute.mTo = readAttributeValue(stream);
                settings.mTotalTransitionTicks = stream.getUnsignedVarInt();
                settings.mCurrentTransitionTicks = stream.getUnsignedVarInt();
                settings.mEasing = (CameraEase) stream.getVarInt();
                settings.mClockName = stream.getString();
                settings.mLocalTransitionTicks = stream.getUnsignedVarInt();
                settings.mNoiseName = stream.getString();
                settings.mNoiseAlignment.mType = (NoiseAlignmentType) stream.getByte();
                settings.mNoiseAlignment.mValue = (int32_t) stream.getUnsignedVarInt();
                break;
            }
            default:
                throw BinaryDataException("Unknown environment attribute payload type");
        }
        return attribute;
    }

    void writeEnvironmentAttribute(BinaryStream &stream, const PacketCodecContext &context,
                                   const EnvironmentAttributeData &attribute) {
        const ProtocolCapabilities &capabilities = context.getCapabilities();
        if (capabilities.mAttributePayloads)
            writePayloadEnvironmentAttribute(stream, attribute);
        else
            writeLegacyEnvironmentAttribute(stream, attribute, capabilities.mAttributeNoiseAlignment);
    }

    EnvironmentAttributeData readEnvironmentAttribute(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
        const ProtocolCapabilities &capabilities = context.getCapabilities();
        if (capabilities.mAttributePayloads)
            return readPayloadEnvironmentAttribute(stream);
        return readLegacyEnvironmentAttribute(stream, capabilities.mAttributeNoiseAlignment);
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
                    writeEnvironmentAttribute(stream, context, attribute);
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
                writeEnvironmentAttribute(stream, context, attribute);
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
                    layer.mAttributes.push_back(readEnvironmentAttribute(stream, context));
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
                mData.mAttributes.push_back(readEnvironmentAttribute(stream, context));
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
