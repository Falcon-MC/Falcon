#include "Plugin/PluginCommand.h"

#include "Core/Debug/BedrockLog.h"

namespace {
    std::string orEmpty(const char *value) {
        return value == nullptr ? std::string() : std::string(value);
    }
}

PluginCommand::PluginCommand(LoadedPlugin &plugin, const std::string &name, const FalconCommandDescriptor &descriptor)
        : Command(name, orEmpty(descriptor.description), orEmpty(descriptor.usage)), mPlugin(plugin),
          mHandler(descriptor.handler), mUserData(descriptor.userData),
          mPermission(descriptor.permission == FALCON_PERMISSION_ANY ? CommandPermission::Any
                                                                     : CommandPermission::GameDirectors) {
}

bool PluginCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (!mPlugin.mEnabled || mHandler == nullptr)
        return false;

    std::vector<const char *> values;
    values.reserve(arguments.size());
    for (const std::string &argument: arguments)
        values.push_back(argument.c_str());

    try {
        return mHandler(reinterpret_cast<FalconCommandSender *>(&sender), values.data(), (uint32_t) values.size(),
                        mUserData) != 0;
    } catch (...) {
        LOG_ERROR(LogAreaID::Server, "[%s] A command handler threw an exception", mPlugin.mDescription.mName.c_str());
        return false;
    }
}

CommandPermission PluginCommand::getRequiredPermission() const {
    return mPermission;
}
