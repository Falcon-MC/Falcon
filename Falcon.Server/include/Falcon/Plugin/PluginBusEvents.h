#pragma once

class EventBus;
class PluginManager;

// each domain hooks the bus signals behind the plugin event types a plugin listens to
namespace PluginBusEvents {
    void hookBrewing(PluginManager &plugins, EventBus &bus);

    void hookActor(PluginManager &plugins, EventBus &bus);

    void hookLevel(PluginManager &plugins, EventBus &bus);

    void hookBlock(PluginManager &plugins, EventBus &bus);

    void hookInventory(PluginManager &plugins, EventBus &bus);

    void hookServer(PluginManager &plugins, EventBus &bus);

    void hookPlayer(PluginManager &plugins, EventBus &bus);

    void hookTeleport(PluginManager &plugins, EventBus &bus);
}
