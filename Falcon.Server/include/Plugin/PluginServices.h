#pragma once

#include <falcon/falcon_api.h>

#include <optional>
#include <string>
#include <unordered_map>

struct LoadedPlugin;

class PluginServices {
public:
    bool registerService(LoadedPlugin &plugin, const std::string &name, FalconServiceHandler handler,
                         void *userData);

    void unregisterService(LoadedPlugin &plugin, const std::string &name);

    bool hasService(const std::string &name) const;

    const LoadedPlugin *getProvider(const std::string &name) const;

    std::optional<std::string> call(const std::string &name, const std::string &request);

    void removePlugin(LoadedPlugin &plugin);

private:
    struct Service {
        LoadedPlugin *mPlugin;
        FalconServiceHandler mHandler;
        void *mUserData;
    };

    std::unordered_map<std::string, Service> mServices;
};
