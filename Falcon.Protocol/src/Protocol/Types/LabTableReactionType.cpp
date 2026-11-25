#include "Protocol/Types/LabTableReactionType.h"

namespace {
    const LabTableReactionType TYPES_BY_ID[] = {
        LabTableReactionType::None,
        LabTableReactionType::IceBomb,
        LabTableReactionType::Bleach,
        LabTableReactionType::ElephantToothpaste,
        LabTableReactionType::Fertilizer,
        LabTableReactionType::HeatBlock,
        LabTableReactionType::MagnesiumSalts,
        LabTableReactionType::MiscFire,
        LabTableReactionType::MiscExplosion,
        LabTableReactionType::MiscLava,
        LabTableReactionType::MiscMystical,
        LabTableReactionType::MiscSmoke,
        LabTableReactionType::MiscLargeSmoke,
    };

    const int32_t TYPE_COUNT = sizeof(TYPES_BY_ID) / sizeof(TYPES_BY_ID[0]);
}

int32_t labTableReactionTypeToId(LabTableReactionType type) {
    for (int32_t i = 0; i < TYPE_COUNT; i++) {
        if (TYPES_BY_ID[i] == type) {
            return i;
        }
    }
    return 0;
}

LabTableReactionType labTableReactionTypeFromId(int32_t id) {
    if (id < 0 || id >= TYPE_COUNT) {
        return LabTableReactionType::None;
    }
    return TYPES_BY_ID[id];
}
