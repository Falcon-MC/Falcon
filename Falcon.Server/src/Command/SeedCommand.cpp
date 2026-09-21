#include "Command/SeedCommand.h"

#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

SeedCommand::SeedCommand(ServerNetworkHandler &handler)
        : Command("seed", "Shows the world seed", "/seed"), mHandler(handler) {}

bool SeedCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    (void) arguments;

    sender.sendTranslation("commands.seed.success", {std::to_string(mHandler.getLevel().getSeed())});
    return true;
}
