#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

enum class DataStoreScalarType {
    Number,
    Bool,
    String,
    StringArray,
};

class DataStoreScalar {
public:
    DataStoreScalarType mType = DataStoreScalarType::Number;
    double mNumberValue = 0.0;
    bool mBoolValue = false;
    std::string mStringValue;
    std::vector<std::string> mStringArrayValue;
};

enum class DataStoreChangeValueType {
    Null,
    Bool,
    Long,
    String,
    Map,
};

class DataStoreChangeValue {
public:
    DataStoreChangeValueType mType = DataStoreChangeValueType::Null;
    bool mBoolValue = false;
    int64_t mLongValue = 0;
    std::string mStringValue;
    std::vector<std::pair<std::string, DataStoreChangeValue>> mMapValue;
};

class DataStoreUpdate {
public:
    std::string mDataStoreName;
    std::string mProperty;
    std::string mPath;
    DataStoreScalar mData;
    int32_t mUpdateCount = 0;
    int32_t mPathUpdateCount = 0;
};

class DataStoreChange {
public:
    std::string mDataStoreName;
    std::string mProperty;
    int32_t mUpdateCount = 0;
    DataStoreChangeValue mNewValue;
};

class DataStoreRemoval {
public:
    std::string mDataStoreName;
};

enum class DataStoreActionType {
    Update,
    Change,
    Removal,
};

class DataStoreAction {
public:
    DataStoreActionType mType = DataStoreActionType::Update;
    DataStoreUpdate mUpdate;
    DataStoreChange mChange;
    DataStoreRemoval mRemoval;
};
