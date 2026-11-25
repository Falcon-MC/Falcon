#include "Core/Utility/EncodingSettings.h"

const EncodingSettings &EncodingSettings::getDefault() {
    static const EncodingSettings settings;
    return settings;
}

const EncodingSettings &EncodingSettings::getServer() {
    static const EncodingSettings settings = []() {
        EncodingSettings server;
        server.mMaxListSize = 1024;
        server.mMaxByteArraySize = 1024 * 512;
        server.mMaxStringLength = 1024 * 32;
        return server;
    }();

    return settings;
}
