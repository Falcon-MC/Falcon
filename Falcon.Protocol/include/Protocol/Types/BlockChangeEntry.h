#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>

enum class BlockChangeMessageType {
    None,
    Create,
    Destroy,
};

class BlockChangeEntry {
public:
    Vector3i mPosition;
    uint32_t mRuntimeId = 0;
    uint32_t mUpdateFlags = 0;
    uint64_t mMessageEntityId = 0;
    BlockChangeMessageType mMessageType = BlockChangeMessageType::None;
};
