#include "Command/AboutCommand.h"

#include "BuildInfo.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <string>

namespace {
    const size_t SHORT_COMMIT_LENGTH = 7;
    const char *UNKNOWN_VALUE = "unknown";

    std::string shortCommit() {
        const std::string commit(FalconBuildInfo::kCommitId);

        if (commit.empty() || commit == UNKNOWN_VALUE)
            return UNKNOWN_VALUE;

        if (commit.size() <= SHORT_COMMIT_LENGTH)
            return commit;

        return commit.substr(0, SHORT_COMMIT_LENGTH);
    }
}

AboutCommand::AboutCommand(ServerNetworkHandler &handler)
        : Command("about", "Shows the server version", "/about", {"version", "ver"}),
          mHandler(handler) {}

CommandPermission AboutCommand::getRequiredPermission() const {
    return CommandPermission::Any;
}

std::vector<CommandOverloadData> AboutCommand::getOverloads() const {
    return {CommandOverloadData()};
}

bool AboutCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    (void) arguments;

    const PingedCompatibleServer &announcement = mHandler.getAnnouncement();

    sender.sendMessage("This server is running Falcon version " + std::string(FalconBuildInfo::kVersion)
                       + " (" + shortCommit() + " on " + FalconBuildInfo::kBranch + ")");
    sender.sendMessage("Minecraft: Bedrock Edition " + announcement.mGameVersion
                       + " (protocol " + std::to_string(announcement.mProtocolVersion) + ")");
    sender.sendMessage("Build " + std::string(FalconBuildInfo::kBuildId) + ", "
                       + FalconBuildInfo::kConfiguration + " configuration");

    return true;
}
