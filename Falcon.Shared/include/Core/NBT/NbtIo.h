#pragma once

#include "Core/NBT/Tag.h"
#include "Core/Utility/BinaryStream.h"

#include <string>

enum class NbtVariant : int {
    BigEndian = 0,
    LittleEndian = 1,
    Network = 2
};

class NbtIo {
public:
    static const int MAX_DEPTH = 16;

    static Tag readTag(ReadOnlyBinaryStream &stream, NbtVariant variant, std::string *outRootName = nullptr);

    static void writeTag(BinaryStream &stream, const Tag &tag, NbtVariant variant,
                         const std::string &rootName = std::string());

    static Tag readValue(ReadOnlyBinaryStream &stream, Tag::Type type, NbtVariant variant, int maxDepth = MAX_DEPTH);

    static void writeValue(BinaryStream &stream, const Tag &tag, NbtVariant variant, int maxDepth = MAX_DEPTH);
};
