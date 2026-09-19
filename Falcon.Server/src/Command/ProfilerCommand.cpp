#include "Command/ProfilerCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"
#include "Server/Profiler.h"

namespace {
    const char *PROFILER_DIRECTORY = "profiler";
}

ProfilerCommand::ProfilerCommand(ServerNetworkHandler &handler)
        : Command("profiler", "Records a server performance report", "/profiler <on|off|status>"),
          mHandler(handler) {}

CommandPermission ProfilerCommand::getRequiredPermission() const {
    return CommandPermission::GameDirectors;
}

std::vector<CommandOverloadData> ProfilerCommand::getOverloads() const {
    CommandParamData actionParameter;
    actionParameter.mName = "action";
    actionParameter.mHasEnumData = true;
    actionParameter.mEnumData.mName = "ProfilerAction";
    actionParameter.mEnumData.mIsSoft = false;
    actionParameter.mEnumData.mValues = {"on", "off", "status"};

    CommandOverloadData overload;
    overload.mParameters.push_back(actionParameter);

    return {overload};
}

bool ProfilerCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    Profiler &profiler = mHandler.getProfiler();
    const std::string &action = arguments[0];

    if (action == "status") {
        if (!profiler.isActive()) {
            sender.sendMessage("Profiler is not running");
            return true;
        }

        sender.sendMessage("Profiler is running, " + std::to_string(profiler.getSampleCount())
                           + " tick(s) recorded since tick " + std::to_string(profiler.getStartTick()));
        return true;
    }

    if (action == "on") {
        if (profiler.isActive()) {
            sender.sendMessage("Profiler is already running");
            return false;
        }

        profiler.start(mHandler.getCurrentTick());
        sender.sendMessage("Profiler started");
        return true;
    }

    if (action == "off") {
        if (!profiler.isActive()) {
            sender.sendMessage("Profiler is not running");
            return false;
        }

        std::string path;
        std::string error;
        if (!profiler.stop(PROFILER_DIRECTORY, path, error)) {
            sender.sendMessage("Could not write the profiler report: " + error);
            return false;
        }

        sender.sendMessage("Profiler stopped, report written to " + path);
        return true;
    }

    sender.sendTranslation("commands.generic.parameter.invalid", {action});
    return false;
}
