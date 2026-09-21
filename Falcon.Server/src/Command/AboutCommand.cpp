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

    sender.sendLocalized("falcon.commands.about.version",
                         {FalconBuildInfo::kVersion, shortCommit(), FalconBuildInfo::kBranch});
    sender.sendLocalized("falcon.commands.about.game",
                         {announcement.mGameVersion, std::to_string(announcement.mProtocolVersion)});
    sender.sendLocalized("falcon.commands.about.build",
                         {FalconBuildInfo::kBuildId, FalconBuildInfo::kConfiguration});

    return true;
}
