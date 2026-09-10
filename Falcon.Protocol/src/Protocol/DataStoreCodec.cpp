#include "Protocol/DataStoreCodec.h"

#include "Core/Utility/BinaryDataException.h"

void DataStoreCodec::writeScalar(BinaryStream &stream, const DataStoreScalar &value) {
    stream.putUnsignedVarInt((uint32_t) value.mType);
    switch (value.mType) {
        case DataStoreScalarType::Number:
            stream.putLDouble(value.mNumberValue);
            break;
        case DataStoreScalarType::Bool:
            stream.putBool(value.mBoolValue);
            break;
        case DataStoreScalarType::String:
            stream.putString(value.mStringValue);
            break;
        case DataStoreScalarType::StringArray:
            stream.putUnsignedVarInt((uint32_t) value.mStringArrayValue.size());
            for (const std::string &entry: value.mStringArrayValue) {
                stream.putString(entry);
            }
            break;
    }
}

DataStoreScalar DataStoreCodec::readScalar(ReadOnlyBinaryStream &stream) {
    DataStoreScalar value;
    uint32_t type = stream.getUnsignedVarInt();
    value.mType = (DataStoreScalarType) type;
    switch (value.mType) {
        case DataStoreScalarType::Number:
            value.mNumberValue = stream.getLDouble();
            break;
        case DataStoreScalarType::Bool:
            value.mBoolValue = stream.getBool();
            break;
        case DataStoreScalarType::String:
            value.mStringValue = stream.getString();
            break;
        case DataStoreScalarType::StringArray: {
            const uint32_t count = stream.getUnsignedVarInt();
            value.mStringArrayValue.reserve(count);
            for (uint32_t i = 0; i < count; i++) {
                value.mStringArrayValue.push_back(stream.getString());
            }
            break;
        }
        default:
            throw BinaryDataException("Invalid data store scalar type " + std::to_string(type));
    }
    return value;
}

void DataStoreCodec::writeChangeValue(BinaryStream &stream, const DataStoreChangeValue &value) {
    switch (value.mType) {
        case DataStoreChangeValueType::Null:
            stream.putLInt(0);
            break;
        case DataStoreChangeValueType::Bool:
            stream.putLInt(1);
            stream.putBool(value.mBoolValue);
            break;
        case DataStoreChangeValueType::Long:
            stream.putLInt(2);
            stream.putLLong((uint64_t) value.mLongValue);
            break;
        case DataStoreChangeValueType::String:
            stream.putLInt(4);
            stream.putString(value.mStringValue);
            break;
        case DataStoreChangeValueType::Map:
            stream.putLInt(6);
            stream.putUnsignedVarInt((uint32_t) value.mMapValue.size());
            for (const std::pair<std::string, DataStoreChangeValue> &entry: value.mMapValue) {
                stream.putString(entry.first);
                writeChangeValue(stream, entry.second);
            }
            break;
    }
}

DataStoreChangeValue DataStoreCodec::readChangeValue(ReadOnlyBinaryStream &stream) {
    DataStoreChangeValue value;
    int32_t type = (int32_t) stream.getLInt();
    switch (type) {
        case 0:
            value.mType = DataStoreChangeValueType::Null;
            break;
        case 1:
            value.mType = DataStoreChangeValueType::Bool;
            value.mBoolValue = stream.getBool();
            break;
        case 2:
            value.mType = DataStoreChangeValueType::Long;
            value.mLongValue = (int64_t) stream.getLLong();
            break;
        case 4:
            value.mType = DataStoreChangeValueType::String;
            value.mStringValue = stream.getString();
            break;
        case 6: {
            value.mType = DataStoreChangeValueType::Map;
            uint32_t size = stream.getUnsignedVarInt();
            value.mMapValue.reserve(size);
            for (uint32_t i = 0; i < size; i++) {
                std::string key = stream.getString();
                value.mMapValue.emplace_back(std::move(key), readChangeValue(stream));
            }
            break;
        }
        default:
            throw BinaryDataException("Invalid data store change value type " + std::to_string(type));
    }
    return value;
}

void DataStoreCodec::writeAction(BinaryStream &stream, const DataStoreAction &action, bool withPathUpdateCount) {
    stream.putUnsignedVarInt((uint32_t) action.mType);
    switch (action.mType) {
        case DataStoreActionType::Update:
            stream.putString(action.mUpdate.mDataStoreName);
            stream.putString(action.mUpdate.mProperty);
            stream.putString(action.mUpdate.mPath);
            writeScalar(stream, action.mUpdate.mData);
            stream.putLInt((uint32_t) action.mUpdate.mUpdateCount);
            if (withPathUpdateCount) {
                stream.putLInt((uint32_t) action.mUpdate.mPathUpdateCount);
            }
            break;
        case DataStoreActionType::Change:
            stream.putString(action.mChange.mDataStoreName);
            stream.putString(action.mChange.mProperty);
            stream.putLInt((uint32_t) action.mChange.mUpdateCount);
            writeChangeValue(stream, action.mChange.mNewValue);
            break;
        case DataStoreActionType::Removal:
            stream.putString(action.mRemoval.mDataStoreName);
            break;
    }
}

DataStoreAction DataStoreCodec::readAction(ReadOnlyBinaryStream &stream, bool withPathUpdateCount) {
    DataStoreAction action;
    uint32_t type = stream.getUnsignedVarInt();
    action.mType = (DataStoreActionType) type;
    switch (action.mType) {
        case DataStoreActionType::Update:
            action.mUpdate.mDataStoreName = stream.getString();
            action.mUpdate.mProperty = stream.getString();
            action.mUpdate.mPath = stream.getString();
            action.mUpdate.mData = readScalar(stream);
            action.mUpdate.mUpdateCount = (int32_t) stream.getLInt();
            if (withPathUpdateCount) {
                action.mUpdate.mPathUpdateCount = (int32_t) stream.getLInt();
            }
            break;
        case DataStoreActionType::Change:
            action.mChange.mDataStoreName = stream.getString();
            action.mChange.mProperty = stream.getString();
            action.mChange.mUpdateCount = (int32_t) stream.getLInt();
            action.mChange.mNewValue = readChangeValue(stream);
            break;
        case DataStoreActionType::Removal:
            action.mRemoval.mDataStoreName = stream.getString();
            break;
        default:
            throw BinaryDataException("Invalid data store action type " + std::to_string(type));
    }
    return action;
}
