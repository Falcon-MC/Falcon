#include "Command/BanIpCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Server/BanList.h"

#include <vector>

namespace {
    bool isIpAddress(const std::string &value) {
        if (value.empty())
            return false;

        for (char character: value) {
            if (!(character >= '0' && character <= '9') && character != '.' && character != ':'
                && !(character >= 'a' && character <= 'f') && !(character >= 'A' && character <= 'F'))
                return false;
        }

        return value.find('.') != std::string::npos || value.find(':') != std::string::npos;
    }
}

BanIpCommand::BanIpCommand(ServerNetworkHandler &handler)
        : Command("ban-ip", "commands.banip.description", "/ban-ip <address|player> [reason]"), mHandler(handler) {}

std::vector<CommandOverloadData> BanIpCommand::getOverloads() const {
    CommandOverloadData byAddress;
    byAddress.mParameters = {makeTypedParameter("address", CommandParamType::String),
                             makeTypedParameter("reason", CommandParamType::Message, true)};

    CommandOverloadData byPlayer;
    byPlayer.mParameters = {makePlayerParameter("player"), makeTypedParameter("reason", CommandParamType::Message, true)};

    return {byPlayer, byAddress};
}

bool BanIpCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    std::string address = arguments[0];
    if (!isIpAddress(address)) {
        ServerPlayer *player = mHandler.getPlayerByName(address);
        if (player == nullptr) {
            sender.sendTranslation("commands.banip.invalid", {});
            return false;
        }
        address = player->getNetworkIdentifier().getAddress();
    }

    const std::string reason = joinArguments(arguments, 1);
    mHandler.getIpBanList().add(address, reason, sender.getSenderName());

    std::vector<std::string> kicked;
    for (auto &entry: mHandler.getPlayers()) {
        if (entry.first.getAddress() != address)
            continue;

        ServerPlayer &player = entry.second;
        kicked.push_back(player.getName());
        mHandler._disconnect(entry.first, reason.empty() ? player.localize("falcon.disconnect.banned")
                                                         : player.localize("falcon.disconnect.bannedReason", {reason}));
    }

    if (kicked.empty()) {
        sender.sendTranslation("commands.banip.success", {address});
        return true;
    }

    std::string names;
    for (const std::string &name: kicked) {
        if (!names.empty())
            names += ", ";
        names += name;
    }

    sender.sendTranslation("commands.banip.success.players", {address, names});
    return true;
}
