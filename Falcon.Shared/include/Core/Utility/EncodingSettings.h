#pragma once

#include <cstddef>

struct EncodingSettings {
    size_t mMaxListSize;
    size_t mMaxByteArraySize;
    size_t mMaxStringLength;

    EncodingSettings() : mMaxListSize(1536), mMaxByteArraySize(1024 * 1024), mMaxStringLength(1024 * 32) {
    }

    static const EncodingSettings &getDefault();

    static const EncodingSettings &getServer();
};
