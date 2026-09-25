#pragma once

#include <falcon/falcon_api.h>

#include <string>

class ServerPlayer;

struct PluginEvent {
    FalconEventType mType = 0;
    bool mCancellable = false;
    bool mCancelled = false;
    bool mMonitor = false;
    ServerPlayer *mPlayer = nullptr;
    std::string *mMessage = nullptr;
};
