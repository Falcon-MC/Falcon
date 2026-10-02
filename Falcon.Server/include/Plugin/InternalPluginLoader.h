#pragma once

class ServerNetworkHandler;
struct LoadedPlugin;

class InternalPluginLoader {
public:
    static bool load(LoadedPlugin &plugin, ServerNetworkHandler &owner);

    static void refresh();

    static void unload(LoadedPlugin &plugin);
};
