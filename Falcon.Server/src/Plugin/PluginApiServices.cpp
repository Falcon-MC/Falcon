#include "Plugin/PluginApiHelpers.h"
#include "Plugin/PluginEvent.h"
#include "Plugin/PluginManager.h"
#include "Plugin/PluginServerApi.h"

#include <optional>
#include <string>

using namespace PluginApiHelpers;

namespace {
    int registerService(FalconPlugin *handle, const char *name, FalconServiceHandler handler, void *userData) {
        LoadedPlugin *source = plugin(handle);
        if (source == nullptr || name == nullptr)
            return 0;

        return PluginManager::getInstance().getServices().registerService(*source, name, handler, userData) ? 1 : 0;
    }

    void unregisterService(FalconPlugin *handle, const char *name) {
        LoadedPlugin *source = plugin(handle);
        if (source == nullptr || name == nullptr)
            return;

        PluginManager::getInstance().getServices().unregisterService(*source, name);
    }

    int hasService(const char *name) {
        if (name == nullptr)
            return 0;

        return PluginManager::getInstance().getServices().hasService(name) ? 1 : 0;
    }

    const char *serviceProvider(const char *name) {
        if (name == nullptr)
            return nullptr;

        const LoadedPlugin *provider = PluginManager::getInstance().getServices().getProvider(name);
        if (provider == nullptr)
            return nullptr;

        return hold(provider->mDescription.mName);
    }

    const char *callService(const char *name, const char *request, int *found) {
        if (found != nullptr)
            *found = 0;
        if (name == nullptr)
            return nullptr;

        const std::optional<std::string> response =
                PluginManager::getInstance().getServices().call(name, request == nullptr ? "" : request);
        if (!response.has_value())
            return nullptr;

        if (found != nullptr)
            *found = 1;
        return hold(*response);
    }

    uint64_t subscribeCustomEvent(FalconPlugin *handle, const char *name, FalconEventPriority priority,
                                  int ignoreCancelled, FalconEventHandler handler, void *userData) {
        LoadedPlugin *source = plugin(handle);
        if (source == nullptr || name == nullptr)
            return 0;

        return PluginManager::getInstance().subscribeCustom(*source, name, priority, ignoreCancelled != 0, handler,
                                                            userData);
    }

    const char *fireCustomEvent(FalconPlugin *handle, const char *name, const char *data, int cancellable,
                                int *cancelled) {
        if (cancelled != nullptr)
            *cancelled = 0;

        LoadedPlugin *source = plugin(handle);
        if (source == nullptr || name == nullptr)
            return nullptr;

        PluginEvent customEvent;
        customEvent.mType = FALCON_EVENT_CUSTOM;
        customEvent.mCancellable = cancellable != 0;
        customEvent.mCause = source->mDescription.mName;
        customEvent.mCustomName = name;
        customEvent.mCustomData = data == nullptr ? "" : data;
        PluginManager::getInstance().dispatch(customEvent);

        if (cancelled != nullptr)
            *cancelled = customEvent.mCancelled ? 1 : 0;
        return hold(customEvent.mCustomData);
    }

    const char *eventName(FalconEvent *target) {
        return hold(event(target)->mCustomName);
    }

    const char *eventData(FalconEvent *target) {
        return hold(event(target)->mCustomData);
    }

    void eventSetData(FalconEvent *target, const char *data) {
        PluginEvent *source = event(target);
        if (source->mMonitor)
            return;

        source->mCustomData = data == nullptr ? "" : data;
    }
}

void PluginServerApi::fillServices(FalconServerApi &api) {
    api.registerService = &registerService;
    api.unregisterService = &unregisterService;
    api.hasService = &hasService;
    api.serviceProvider = &serviceProvider;
    api.callService = &callService;
    api.subscribeCustomEvent = &subscribeCustomEvent;
    api.fireCustomEvent = &fireCustomEvent;
    api.eventName = &eventName;
    api.eventData = &eventData;
    api.eventSetData = &eventSetData;
}
