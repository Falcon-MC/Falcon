#include "Command/StopCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"

StopCommand::StopCommand(ServerNetworkHandler &handler)
        : Command("stop", "commands.stop.description", "/stop"), mHandler(handler) {}

bool StopCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    (void) arguments;

    sender.sendTranslation("commands.stop.start", {});
    mHandler.requestStop();
    return true;
}
