#include "Server/Commands/DebugHudCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "Server/Debug/DebugHud.h"
#include "World/Actor/Player/ServerPlayer.h"

DebugHudCommand::DebugHudCommand(ServerNetworkHandler &handler)
        : Command("debughud", "Shows live server numbers in a boss bar",
                  "/debughud <performance|generation|network|memory|off>"),
          mHandler(handler) {}

std::vector<CommandOverloadData> DebugHudCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makeEnumParameter("page", "DebugHudPage",
                                                     {"performance", "generation", "network", "memory", "off"}));
    return {overload};
}

bool DebugHudCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    if (!sender.isPlayer()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    DebugHud::Page page;
    if (arguments[0] == "performance")
        page = DebugHud::Page::Performance;
    else if (arguments[0] == "generation")
        page = DebugHud::Page::Generation;
    else if (arguments[0] == "network")
        page = DebugHud::Page::Network;
    else if (arguments[0] == "memory")
        page = DebugHud::Page::Memory;
    else if (arguments[0] == "off")
        page = DebugHud::Page::Off;
    else {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    ServerPlayer *player = sender.asPlayer();
    player->getDebugHud().setPage(*player, page);
    return true;
}
