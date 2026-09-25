#include "Plugin/PluginServices.h"

#include "Core/Debug/BedrockLog.h"
#include "Plugin/LoadedPlugin.h"

bool PluginServices::registerService(LoadedPlugin &plugin, const std::string &name, FalconServiceHandler handler,
                                     void *userData) {
    if (name.empty() || handler == nullptr)
        return false;

    const auto existing = mServices.find(name);
    if (existing != mServices.end() && existing->second.mPlugin != &plugin)
        return false;

    mServices[name] = Service{&plugin, handler, userData};
    return true;
}

void PluginServices::unregisterService(LoadedPlugin &plugin, const std::string &name) {
    const auto it = mServices.find(name);
    if (it != mServices.end() && it->second.mPlugin == &plugin)
        mServices.erase(it);
}

bool PluginServices::hasService(const std::string &name) const {
    const auto it = mServices.find(name);
    return it != mServices.end() && it->second.mPlugin->mEnabled;
}

const LoadedPlugin *PluginServices::getProvider(const std::string &name) const {
    const auto it = mServices.find(name);
    if (it == mServices.end())
        return nullptr;

    return it->second.mPlugin;
}

std::optional<std::string> PluginServices::call(const std::string &name, const std::string &request) {
    static constexpr int MAX_CALL_DEPTH = 16;
    thread_local int depth = 0;

    const auto it = mServices.find(name);
    if (it == mServices.end() || !it->second.mPlugin->mEnabled || depth >= MAX_CALL_DEPTH)
        return std::nullopt;

    const Service service = it->second;
    std::string response;

    depth++;
    try {
        const char *result = service.mHandler(request.c_str(), service.mUserData);
        if (result != nullptr)
            response = result;
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] Service %s threw an exception",
                  service.mPlugin->mDescription.mName.c_str(), name.c_str());
    }
    depth--;

    return response;
}

void PluginServices::removePlugin(LoadedPlugin &plugin) {
    for (auto it = mServices.begin(); it != mServices.end();) {
        if (it->second.mPlugin == &plugin) {
            it = mServices.erase(it);
        } else {
            ++it;
        }
    }
}
