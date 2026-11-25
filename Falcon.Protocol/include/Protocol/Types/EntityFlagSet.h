#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

#include <vector>

class EntityFlagCodec {
public:
    static void writeFlags(BinaryStream &stream, const std::vector<bool> &flags);

    static std::vector<bool> readFlags(ReadOnlyBinaryStream &stream);
};
