#pragma once

#include <cstdint>

enum class LabTableReactionType {
    None,
    IceBomb,
    Bleach,
    ElephantToothpaste,
    Fertilizer,
    HeatBlock,
    MagnesiumSalts,
    MiscFire,
    MiscExplosion,
    MiscLava,
    MiscMystical,
    MiscSmoke,
    MiscLargeSmoke,
};

int32_t labTableReactionTypeToId(LabTableReactionType type);

LabTableReactionType labTableReactionTypeFromId(int32_t id);
