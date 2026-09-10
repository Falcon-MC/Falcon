#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <string>

enum class MemoryCategory {
    Unknown,
};

class MemoryCategoryCounter {
public:
    int32_t mCategory = 0;
    int64_t mCurrentBytes = 0;
};

class EntityDiagnosticTimingInfo {
public:
    std::string mDisplayName;
    std::string mEntity;
    int64_t mTimeInNs = 0;
    int8_t mPercentOfTotal = 0;
    Vector3f mPosition;
    std::string mDimension;
};

class SystemDiagnosticTimingInfo {
public:
    std::string mDisplayName;
    int64_t mSystemIndex = 0;
    int64_t mTimeInNs = 0;
    int8_t mPercentOfTotal = 0;
};

class SystemCategory {
public:
    std::string mCategoryName;
    int64_t mSystemIndex = 0;
};

class WhiskerScopeDataSummary {
public:
    std::string mLabel;
    std::string mIndentation;
    int64_t mTotalHighCostNs = 0;
    int64_t mTotalMidCostNs = 0;
    int64_t mTotalLowCostNs = 0;
};
