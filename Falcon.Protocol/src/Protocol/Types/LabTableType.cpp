#include "Protocol/Types/LabTableType.h"

int32_t labTableTypeToId(LabTableType type) {
    return (int32_t) type;
}

LabTableType labTableTypeFromId(int32_t id) {
    switch (id) {
        case 0:
            return LabTableType::StartCombine;
        case 1:
            return LabTableType::StartReaction;
        case 2:
            return LabTableType::Reset;
        default:
            return LabTableType::StartCombine;
    }
}
