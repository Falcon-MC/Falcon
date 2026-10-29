#include "Server/Commands/SeedCommand.h"

#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

SeedCommand::SeedCommand(ServerNetworkHandler &handler)
        : Command("seed", "Shows the world seed", "/seed"), mHandler(handler) {}

bool SeedCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    (void) arguments;

    sender.sendTranslation("commands.seed.success", {std::to_string(mHandler.getWorldOf(sender.getLevel()).getSeed())});
    return true;
}
