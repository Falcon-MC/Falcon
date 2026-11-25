#include "Network/Auth/XboxLiveConfig.h"

namespace {

    XboxLiveConfig makeConfig(const char *clientId, const char *deviceType, const char *version,
                              const char *userAgent) {
        XboxLiveConfig config;
        config.mClientId = clientId;
        config.mDeviceType = deviceType;
        config.mVersion = version;
        config.mUserAgent = userAgent;
        return config;
    }

}

XboxLiveConfig XboxLiveConfig::android() {
    return makeConfig("0000000048183522", "Android", "8.0.0", "XAL Android 2020.07.20200714.000");
}

XboxLiveConfig XboxLiveConfig::ios() {
    return makeConfig("000000004c17c01a", "iOS", "15.6.1", "XAL iOS 2021.11.20211021.000");
}

XboxLiveConfig XboxLiveConfig::win32() {
    return makeConfig("0000000040159362", "Win32", "10.0.25398.4909", "XAL Win32 2021.11.20220411.002");
}

XboxLiveConfig XboxLiveConfig::nintendo() {
    return makeConfig("00000000441cc96b", "Nintendo", "0.0.0", "XAL");
}

XboxLiveConfig XboxLiveConfig::playStation() {
    return makeConfig("000000004827c78e", "Playstation", "10.0.0", "XAL");
}
