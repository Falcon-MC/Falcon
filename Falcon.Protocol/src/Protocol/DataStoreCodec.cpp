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
            const uint32_t count = stream.getArrayLength();
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
        case DataStoreChangeValueType::Double:
            stream.putLInt(3);
            stream.putLDouble(value.mDoubleValue);
            break;
        case DataStoreChangeValueType::String:
            stream.putLInt(4);
            stream.putString(value.mStringValue);
            break;
        case DataStoreChangeValueType::Array:
            stream.putLInt(5);
            stream.putUnsignedVarInt((uint32_t) value.mArrayValue.size());
            for (const DataStoreChangeValue &entry: value.mArrayValue) {
                writeChangeValue(stream, entry);
            }
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

DataStoreChangeValue DataStoreCodec::readChangeValue(ReadOnlyBinaryStream &stream, int depth) {
    // map and array values nest another change value, so a crafted chain would recurse without this bound
    static const int MAX_DEPTH = 16;
    if (depth > MAX_DEPTH)
        throw BinaryDataException("Data store change value is nested too deeply");

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
        case 3:
            value.mType = DataStoreChangeValueType::Double;
            value.mDoubleValue = stream.getLDouble();
            break;
        case 4:
            value.mType = DataStoreChangeValueType::String;
            value.mStringValue = stream.getString();
            break;
        case 5: {
            value.mType = DataStoreChangeValueType::Array;
            uint32_t size = stream.getArrayLength();
            value.mArrayValue.reserve(size);
            for (uint32_t i = 0; i < size; i++) {
                value.mArrayValue.push_back(readChangeValue(stream, depth + 1));
            }
            break;
        }
        case 6: {
            value.mType = DataStoreChangeValueType::Map;
            uint32_t size = stream.getArrayLength();
            value.mMapValue.reserve(size);
            for (uint32_t i = 0; i < size; i++) {
                std::string key = stream.getString();
                value.mMapValue.emplace_back(std::move(key), readChangeValue(stream, depth + 1));
            }
            break;
        }
        default:
            throw BinaryDataException("Invalid data store change value type " + std::to_string(type));
    }
    return value;
}

void DataStoreCodec::writeUpdate(BinaryStream &stream, const DataStoreUpdate &update, bool variables) {
    if (variables) {
        stream.putString(update.mVariableName);
        writeScalar(stream, update.mData);
        stream.putLInt((uint32_t) update.mUpdateCount);
        return;
    }

    stream.putString(update.mDataStoreName);
    stream.putString(update.mProperty);
    stream.putString(update.mPath);
    writeScalar(stream, update.mData);
    stream.putLInt((uint32_t) update.mUpdateCount);
    stream.putLInt((uint32_t) update.mPathUpdateCount);
}

DataStoreUpdate DataStoreCodec::readUpdate(ReadOnlyBinaryStream &stream, bool variables) {
    DataStoreUpdate update;
    if (variables) {
        update.mVariableName = stream.getString();
        update.mData = readScalar(stream);
        update.mUpdateCount = (int32_t) stream.getLInt();
        return update;
    }

    update.mDataStoreName = stream.getString();
    update.mProperty = stream.getString();
    update.mPath = stream.getString();
    update.mData = readScalar(stream);
    update.mUpdateCount = (int32_t) stream.getLInt();
    update.mPathUpdateCount = (int32_t) stream.getLInt();
    return update;
}

void DataStoreCodec::writeAction(BinaryStream &stream, const DataStoreAction &action, bool variables) {
    stream.putUnsignedVarInt((uint32_t) action.mType);
    switch (action.mType) {
        case DataStoreActionType::Update:
            writeUpdate(stream, action.mUpdate, variables);
            break;
        case DataStoreActionType::Change:
            if (variables) {
                // the count comes before the value here, after it in an update
                stream.putString(action.mChange.mVariableName);
                stream.putLInt((uint32_t) action.mChange.mUpdateCount);
                writeScalar(stream, action.mChange.mNewVariableValue);
                break;
            }
            stream.putString(action.mChange.mDataStoreName);
            stream.putString(action.mChange.mProperty);
            stream.putLInt((uint32_t) action.mChange.mUpdateCount);
            writeChangeValue(stream, action.mChange.mNewValue);
            break;
        case DataStoreActionType::Removal:
            stream.putString(variables ? action.mRemoval.mVariableName : action.mRemoval.mDataStoreName);
            break;
    }
}

DataStoreAction DataStoreCodec::readAction(ReadOnlyBinaryStream &stream, bool variables) {
    DataStoreAction action;
    uint32_t type = stream.getUnsignedVarInt();
    action.mType = (DataStoreActionType) type;
    switch (action.mType) {
        case DataStoreActionType::Update:
            action.mUpdate = readUpdate(stream, variables);
            break;
        case DataStoreActionType::Change:
            if (variables) {
                action.mChange.mVariableName = stream.getString();
                action.mChange.mUpdateCount = (int32_t) stream.getLInt();
                action.mChange.mNewVariableValue = readScalar(stream);
                break;
            }
            action.mChange.mDataStoreName = stream.getString();
            action.mChange.mProperty = stream.getString();
            action.mChange.mUpdateCount = (int32_t) stream.getLInt();
            action.mChange.mNewValue = readChangeValue(stream);
            break;
        case DataStoreActionType::Removal:
            if (variables)
                action.mRemoval.mVariableName = stream.getString();
            else
                action.mRemoval.mDataStoreName = stream.getString();
            break;
        default:
            throw BinaryDataException("Invalid data store action type " + std::to_string(type));
    }
    return action;
}
