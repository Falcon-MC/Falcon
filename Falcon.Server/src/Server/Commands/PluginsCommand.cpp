#include "Server/Commands/PluginsCommand.h"

#include "Falcon/Plugin/PluginManager.h"
#include "Network/ServerNetworkHandler.h"

#include <string>

namespace {
    const char *ENABLED_COLOR = "§a";
    const char *DISABLED_COLOR = "§c";
    const char *SEPARATOR = "§f, ";

    std::string describe(const LoadedPlugin &plugin) {
        std::string text = plugin.mEnabled ? ENABLED_COLOR : DISABLED_COLOR;
        text += plugin.mDescription.mName;

        if (!plugin.mDescription.mVersion.empty())
            text += " " + plugin.mDescription.mVersion;

        return text;
    }
}

PluginsCommand::PluginsCommand(ServerNetworkHandler &handler)
        : Command("plugins", "Lists the plugins of the server", "/plugins", {"pl"}), mHandler(handler) {}

CommandPermission PluginsCommand::getRequiredPermission() const {
    return CommandPermission::Any;
}

std::vector<CommandOverloadData> PluginsCommand::getOverloads() const {
    return {CommandOverloadData()};
}

bool PluginsCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    (void) arguments;

    const auto &plugins = mHandler.getPluginManager().getPlugins();
    std::string names;

    for (const auto &plugin: plugins) {
        if (!names.empty())
            names += SEPARATOR;

        names += describe(*plugin);
    }

    sender.sendLocalized("falcon.commands.plugins.list", {std::to_string(plugins.size()), names});
    return true;
}
