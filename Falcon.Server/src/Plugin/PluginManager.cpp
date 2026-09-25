#include "Plugin/PluginManager.h"

#include "Actor/ServerPlayer.h"
#include "Core/Debug/BedrockLog.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginCommand.h"
#include "Plugin/PluginPackets.h"
#include "Plugin/PluginServerApi.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace {
    PluginManager *gInstance = nullptr;

    std::string toLower(std::string value) {
        for (char &character: value) {
            if (character >= 'A' && character <= 'Z')
                character = (char) (character - 'A' + 'a');
        }
        return value;
    }
}

PluginManager::PluginManager(ServerNetworkHandler &owner) : mOwner(owner) {
    gInstance = this;
    _hookEventBus();
    mPermissions = std::make_unique<PluginPermissions>(mOwner.getEventBus());
}

PluginManager::~PluginManager() {
    if (mOwner.isTransportReady())
        mOwner.getNetworkHandler().setOutboundFilter(nullptr);

    EventBus &bus = mOwner.getEventBus();
    bus.after().mPlayerJoin.unsubscribe(mJoinHook);
    bus.after().mPlayerLeave.unsubscribe(mQuitHook);
    bus.before().mChatSend.unsubscribe(mChatHook);

    disableAll();
    mScheduler.shutdown();
    mSubscriptions.clear();
    mPlugins.clear();
    if (gInstance == this)
        gInstance = nullptr;
}

PluginManager &PluginManager::getInstance() {
    return *gInstance;
}

LoadedPlugin *PluginManager::fromHandle(FalconPlugin *plugin) {
    return reinterpret_cast<LoadedPlugin *>(plugin);
}

FalconPlugin *PluginManager::toHandle(LoadedPlugin &plugin) {
    return reinterpret_cast<FalconPlugin *>(&plugin);
}

void PluginManager::loadAll(const std::string &directory) {
    namespace fs = std::filesystem;

    std::error_code errorCode;
    fs::create_directories(directory, errorCode);

    std::unordered_set<std::string> names;
    for (const fs::directory_entry &entry: fs::directory_iterator(directory, errorCode)) {
        if (!entry.is_directory())
            continue;

        const fs::path manifest = entry.path() / "plugin.json";
        if (!fs::exists(manifest))
            continue;

        auto plugin = std::make_unique<LoadedPlugin>();
        std::string error;
        if (!PluginDescription::load(manifest.string(), plugin->mDescription, error)) {
            LOG_ERROR(LogAreaID::Server, "Could not load plugin in %s: %s", entry.path().string().c_str(),
                      error.c_str());
            continue;
        }

        const PluginDescription &description = plugin->mDescription;
        if (!names.insert(toLower(description.mName)).second) {
            LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: another plugin has the same name",
                      description.mName.c_str());
            continue;
        }

        if (description.mApiMajor != FALCON_API_VERSION_MAJOR || description.mApiMinor > FALCON_API_VERSION_MINOR) {
            LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: it needs API %u.%u, the server provides %u.%u",
                      description.mName.c_str(), description.mApiMajor, description.mApiMinor,
                      FALCON_API_VERSION_MAJOR, FALCON_API_VERSION_MINOR);
            continue;
        }

        if (description.mRuntime != "native") {
            LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: the %s runtime is not supported yet",
                      description.mName.c_str(), description.mRuntime.c_str());
            continue;
        }

        plugin->mDirectory = entry.path().string();
        plugin->mDataFolder = (entry.path() / "data").string();
        fs::create_directories(plugin->mDataFolder, errorCode);
        mPlugins.push_back(std::move(plugin));
    }

    _sortByDependencies();

    std::vector<std::unique_ptr<LoadedPlugin>> loaded;
    std::unordered_set<std::string> loadedNames;
    for (std::unique_ptr<LoadedPlugin> &plugin: mPlugins) {
        bool dependenciesLoaded = true;
        for (const std::string &dependency: plugin->mDescription.mDepend) {
            if (loadedNames.count(toLower(dependency)) == 0) {
                LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: dependency %s failed to load",
                          plugin->mDescription.mName.c_str(), dependency.c_str());
                dependenciesLoaded = false;
            }
        }

        if (!dependenciesLoaded || !_loadNative(*plugin))
            continue;

        loadedNames.insert(toLower(plugin->mDescription.mName));
        loaded.push_back(std::move(plugin));
    }
    mPlugins.swap(loaded);

    for (std::unique_ptr<LoadedPlugin> &plugin: mPlugins) {
        const fs::path resourcePack = fs::path(plugin->mDirectory) / "resource_pack";
        if (fs::exists(resourcePack / "manifest.json", errorCode))
            mOwner.getResourcePacks().loadFolder(resourcePack.string(), plugin->mDescription.mName);
    }

    for (std::unique_ptr<LoadedPlugin> &plugin: mPlugins) {
        if (plugin->mCallbacks.onLoad == nullptr)
            continue;

        try {
            plugin->mCallbacks.onLoad(plugin->mCallbacks.userData);
        } catch (...) {
            LOG_ERROR(LogAreaID::Server, "[%s] onLoad threw an exception", plugin->mDescription.mName.c_str());
        }
    }
}

bool PluginManager::_loadNative(LoadedPlugin &plugin) {
    const std::string &name = plugin.mDescription.mName;
    const std::string path = (std::filesystem::path(plugin.mDirectory)
                              / NativeLibrary::fileName(plugin.mDescription.mMain)).string();

    plugin.mLibrary = std::make_unique<NativeLibrary>();
    std::string error;
    if (!plugin.mLibrary->open(path, error)) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s from %s: %s", name.c_str(), path.c_str(),
                  error.c_str());
        return false;
    }

    const auto entry = reinterpret_cast<FalconPluginEntry>(plugin.mLibrary->symbol(FALCON_PLUGIN_ENTRY_NAME));
    if (entry == nullptr) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: %s is not exported", name.c_str(),
                  FALCON_PLUGIN_ENTRY_NAME);
        return false;
    }

    int accepted = 0;
    try {
        accepted = entry(&PluginServerApi::get(), toHandle(plugin), &plugin.mCallbacks);
    } catch (...) {
        accepted = 0;
    }

    if (accepted == 0) {
        LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: it refused to initialise", name.c_str());
        return false;
    }

    LOG_INFO(LogAreaID::Server, "Loaded plugin %s %s", name.c_str(), plugin.mDescription.mVersion.c_str());
    return true;
}

void PluginManager::_sortByDependencies() {
    std::unordered_map<std::string, LoadedPlugin *> byName;
    for (std::unique_ptr<LoadedPlugin> &plugin: mPlugins)
        byName[toLower(plugin->mDescription.mName)] = plugin.get();

    struct Edge {
        LoadedPlugin *mPlugin;
        bool mHard;
    };

    std::unordered_map<LoadedPlugin *, std::vector<Edge>> before;
    std::unordered_set<LoadedPlugin *> rejected;
    for (std::unique_ptr<LoadedPlugin> &plugin: mPlugins) {
        const PluginDescription &description = plugin->mDescription;

        for (const std::string &dependency: description.mDepend) {
            const auto it = byName.find(toLower(dependency));
            if (it == byName.end()) {
                LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: missing dependency %s",
                          description.mName.c_str(), dependency.c_str());
                rejected.insert(plugin.get());
                continue;
            }
            before[plugin.get()].push_back(Edge{it->second, true});
        }

        for (const std::string &dependency: description.mSoftDepend) {
            const auto it = byName.find(toLower(dependency));
            if (it != byName.end())
                before[plugin.get()].push_back(Edge{it->second, false});
        }

        for (const std::string &dependent: description.mLoadBefore) {
            const auto it = byName.find(toLower(dependent));
            if (it != byName.end())
                before[it->second].push_back(Edge{plugin.get(), false});
        }
    }

    std::vector<std::unique_ptr<LoadedPlugin>> sorted;
    std::unordered_set<LoadedPlugin *> placed;
    std::unordered_set<LoadedPlugin *> visiting;

    std::function<bool(LoadedPlugin *)> visit = [&](LoadedPlugin *plugin) {
        if (placed.count(plugin) != 0)
            return true;
        if (rejected.count(plugin) != 0)
            return false;

        visiting.insert(plugin);
        for (const Edge &edge: before[plugin]) {
            const char *name = plugin->mDescription.mName.c_str();
            const char *dependencyName = edge.mPlugin->mDescription.mName.c_str();

            if (visiting.count(edge.mPlugin) != 0) {
                if (!edge.mHard)
                    continue;

                LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: circular dependency on %s", name,
                          dependencyName);
                rejected.insert(plugin);
                visiting.erase(plugin);
                return false;
            }

            if (!visit(edge.mPlugin) && edge.mHard) {
                LOG_ERROR(LogAreaID::Server, "Could not load plugin %s: dependency %s failed", name, dependencyName);
                rejected.insert(plugin);
                visiting.erase(plugin);
                return false;
            }
        }

        visiting.erase(plugin);
        placed.insert(plugin);
        for (std::unique_ptr<LoadedPlugin> &owned: mPlugins) {
            if (owned.get() == plugin)
                sorted.push_back(std::move(owned));
        }
        return true;
    };

    for (size_t index = 0; index < mPlugins.size(); index++) {
        LoadedPlugin *plugin = mPlugins[index].get();
        if (plugin != nullptr)
            visit(plugin);
    }

    mPlugins.swap(sorted);
}

void PluginManager::enableAll() {
    for (std::unique_ptr<LoadedPlugin> &plugin: mPlugins) {
        const std::string &name = plugin->mDescription.mName;

        bool dependenciesReady = true;
        for (const std::string &dependency: plugin->mDescription.mDepend) {
            const auto it = std::find_if(mPlugins.begin(), mPlugins.end(),
                                         [&](const std::unique_ptr<LoadedPlugin> &other) {
                                             return toLower(other->mDescription.mName) == toLower(dependency);
                                         });
            if (it == mPlugins.end() || !(*it)->mEnabled)
                dependenciesReady = false;
        }
        if (!dependenciesReady) {
            LOG_ERROR(LogAreaID::Server, "Could not enable plugin %s: a dependency is not enabled", name.c_str());
            continue;
        }

        plugin->mEnabled = true;
        bool enabled = false;
        try {
            const FalconPluginCallbacks &callbacks = plugin->mCallbacks;
            enabled = callbacks.onEnable == nullptr || callbacks.onEnable(callbacks.userData) != 0;
        } catch (...) {
            enabled = false;
        }

        if (!enabled) {
            LOG_ERROR(LogAreaID::Server, "Could not enable plugin %s", name.c_str());
            plugin->mEnabled = false;
            _disable(*plugin);
            continue;
        }

        LOG_INFO(LogAreaID::Server, "Enabled plugin %s %s", name.c_str(), plugin->mDescription.mVersion.c_str());
    }
}

void PluginManager::disableAll() {
    for (auto it = mPlugins.rbegin(); it != mPlugins.rend(); ++it) {
        if ((*it)->mEnabled)
            _disable(**it);
    }
}

void PluginManager::_disable(LoadedPlugin &plugin) {
    if (plugin.mEnabled && plugin.mCallbacks.onDisable != nullptr) {
        try {
            plugin.mCallbacks.onDisable(plugin.mCallbacks.userData);
        } catch (...) {
            LOG_ERROR(LogAreaID::Server, "[%s] onDisable threw an exception", plugin.mDescription.mName.c_str());
        }
    }

    plugin.mEnabled = false;
    mScheduler.cancelAll(plugin);

    for (const std::string &command: plugin.mCommands)
        mOwner.getCommands().unregisterCommand(command);
    plugin.mCommands.clear();
    mSubscriptions.erase(std::remove_if(mSubscriptions.begin(), mSubscriptions.end(), [&](const Subscription &entry) {
        return entry.mPlugin == &plugin;
    }), mSubscriptions.end());
    _updateSubscribedTypes();
    mPermissions->clearPlugin(plugin);
}

void PluginManager::_updateSubscribedTypes() {
    uint64_t types = 0;
    for (const Subscription &subscription: mSubscriptions) {
        if (subscription.mType < 64)
            types |= (uint64_t) 1 << subscription.mType;
    }
    mSubscribedTypes = types;

    if (!mOwner.isTransportReady())
        return;

    if (!hasSubscribers(FALCON_EVENT_DATA_PACKET_SEND)) {
        mOwner.getNetworkHandler().setOutboundFilter(nullptr);
        return;
    }

    mOwner.getNetworkHandler().setOutboundFilter([this](const NetworkIdentifier &id, std::string &data) {
        return PluginPackets::onSend(*this, id, data);
    });
}

void PluginManager::tick() {
    mScheduler.tick();
}

uint64_t PluginManager::subscribe(LoadedPlugin &plugin, FalconEventType type, FalconEventPriority priority,
                                  bool ignoreCancelled, FalconEventHandler handler, void *userData) {
    if (handler == nullptr || priority > FALCON_PRIORITY_MONITOR)
        return 0;

    const uint64_t id = mNextSubscriptionId++;
    const Subscription subscription{id, &plugin, type, priority, ignoreCancelled, handler, userData};
    const auto position = std::upper_bound(mSubscriptions.begin(), mSubscriptions.end(), subscription,
                                           [](const Subscription &left, const Subscription &right) {
                                               return left.mPriority < right.mPriority;
                                           });
    mSubscriptions.insert(position, subscription);
    _updateSubscribedTypes();
    return id;
}

void PluginManager::unsubscribe(uint64_t id) {
    mSubscriptions.erase(std::remove_if(mSubscriptions.begin(), mSubscriptions.end(), [id](const Subscription &entry) {
        return entry.mId == id;
    }), mSubscriptions.end());
    _updateSubscribedTypes();
}

bool PluginManager::registerCommand(LoadedPlugin &plugin, const FalconCommandDescriptor &descriptor) {
    if (descriptor.name == nullptr || descriptor.handler == nullptr)
        return false;

    std::string name = toLower(descriptor.name);
    if (name.empty() || name.find(' ') != std::string::npos)
        return false;

    CommandMap &commands = mOwner.getCommands();
    if (commands.getCommand(name) != nullptr)
        name = toLower(plugin.mDescription.mName) + ":" + name;
    if (commands.getCommand(name) != nullptr)
        return false;

    commands.registerCommand(std::make_shared<PluginCommand>(plugin, name, descriptor));
    plugin.mCommands.push_back(name);
    return true;
}

void PluginManager::dispatch(PluginEvent &event) {
    static constexpr int MAX_DISPATCH_DEPTH = 8;
    thread_local int depth = 0;

    if (!hasSubscribers(event.mType) || depth >= MAX_DISPATCH_DEPTH)
        return;

    depth++;
    const std::vector<Subscription> snapshot = mSubscriptions;

    for (const Subscription &subscription: snapshot) {
        if (subscription.mType != event.mType || !subscription.mPlugin->mEnabled)
            continue;

        event.mMonitor = subscription.mPriority == FALCON_PRIORITY_MONITOR;

        if (subscription.mIgnoreCancelled && event.mCancelled)
            continue;

        try {
            subscription.mHandler(reinterpret_cast<FalconEvent *>(&event), subscription.mUserData);
        } catch (...) {
            LOG_ERROR(LogAreaID::Server, "[%s] An event handler threw an exception",
                      subscription.mPlugin->mDescription.mName.c_str());
        }
    }

    event.mMonitor = false;
    depth--;
}

void PluginManager::_hookEventBus() {
    EventBus &bus = mOwner.getEventBus();

    mJoinHook = bus.after().mPlayerJoin.subscribe([this](PlayerJoinAfterEvent &source) {
        PluginEvent event;
        event.mType = FALCON_EVENT_PLAYER_JOIN;
        event.mPlayer = &source.mPlayer;
        dispatch(event);
    });

    mQuitHook = bus.after().mPlayerLeave.subscribe([this](PlayerLeaveAfterEvent &source) {
        if (source.mPlayer == nullptr)
            return;

        PluginEvent event;
        event.mType = FALCON_EVENT_PLAYER_QUIT;
        event.mPlayer = source.mPlayer;
        dispatch(event);
    });

    mChatHook = bus.before().mChatSend.subscribe([this](PlayerChatBeforeEvent &source) {
        PluginEvent event;
        event.mType = FALCON_EVENT_PLAYER_CHAT;
        event.mCancellable = true;
        event.mCancelled = source.isCancelled();
        event.mPlayer = &source.mSender;
        event.mMessage = &source.mMessage;
        dispatch(event);
        source.setCancelled(event.mCancelled);
    });
}
