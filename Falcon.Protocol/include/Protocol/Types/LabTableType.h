#pragma once

#include <cstdint>

enum class LabTableType {
    StartCombine,
    StartReaction,
    Reset,
};

int32_t labTableTypeToId(LabTableType type);

LabTableType labTableTypeFromId(int32_t id);
