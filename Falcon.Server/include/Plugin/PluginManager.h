#pragma once

#include "Plugin/LoadedPlugin.h"
#include "Plugin/PluginEvent.h"
#include "Plugin/PluginPermissions.h"
#include "Plugin/PluginScheduler.h"

#include <falcon/falcon_api.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ServerNetworkHandler;

class PluginManager {
public:
    explicit PluginManager(ServerNetworkHandler &owner);

    ~PluginManager();

    static PluginManager &getInstance();

    static LoadedPlugin *fromHandle(FalconPlugin *plugin);

    static FalconPlugin *toHandle(LoadedPlugin &plugin);

    void loadAll(const std::string &directory);

    void enableAll();

    void disableAll();

    void tick();

    uint64_t subscribe(LoadedPlugin &plugin, FalconEventType type, FalconEventPriority priority, bool ignoreCancelled,
                       FalconEventHandler handler, void *userData);

    void unsubscribe(uint64_t id);

    bool registerCommand(LoadedPlugin &plugin, const FalconCommandDescriptor &descriptor);

    void dispatch(PluginEvent &event);

    ServerNetworkHandler &getOwner() {
        return mOwner;
    }

    PluginScheduler &getScheduler() {
        return mScheduler;
    }

    PluginPermissions &getPermissions() {
        return *mPermissions;
    }

    bool hasSubscribers(FalconEventType type) const {
        return type < 64 && (mSubscribedTypes & ((uint64_t) 1 << type)) != 0;
    }

private:
    struct Subscription {
        uint64_t mId;
        LoadedPlugin *mPlugin;
        FalconEventType mType;
        FalconEventPriority mPriority;
        bool mIgnoreCancelled;
        FalconEventHandler mHandler;
        void *mUserData;
    };

    bool _loadNative(LoadedPlugin &plugin);

    void _sortByDependencies();

    void _hookEventBus();

    void _disable(LoadedPlugin &plugin);

    void _updateSubscribedTypes();

    ServerNetworkHandler &mOwner;
    PluginScheduler mScheduler;
    std::unique_ptr<PluginPermissions> mPermissions;
    uint64_t mSubscribedTypes = 0;
    std::vector<std::unique_ptr<LoadedPlugin>> mPlugins;
    std::vector<Subscription> mSubscriptions;
    uint64_t mNextSubscriptionId = 1;
    uint32_t mJoinHook = 0;
    uint32_t mQuitHook = 0;
    uint32_t mChatHook = 0;
};
