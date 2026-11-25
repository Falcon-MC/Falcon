#pragma once

#include <string>

struct XboxLiveConfig {
    std::string mClientId;
    std::string mDeviceType;
    std::string mVersion;
    std::string mUserAgent;

    static XboxLiveConfig android();

    static XboxLiveConfig ios();

    static XboxLiveConfig win32();

    static XboxLiveConfig nintendo();

    static XboxLiveConfig playStation();

    bool operator==(const XboxLiveConfig &right) const {
        return mClientId == right.mClientId && mDeviceType == right.mDeviceType && mVersion == right.mVersion &&
               mUserAgent == right.mUserAgent;
    }
};
