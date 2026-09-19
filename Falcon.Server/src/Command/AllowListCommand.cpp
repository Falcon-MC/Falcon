#include "Command/AllowListCommand.h"

#include "Core/Text/StringUtil.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    CommandParamData makeActionParameter(const std::string &enumName, const std::vector<std::string> &actions) {
        CommandParamData parameter;
        parameter.mName = "action";
        parameter.mHasEnumData = true;
        parameter.mEnumData.mName = enumName;
        parameter.mEnumData.mIsSoft = false;
        parameter.mEnumData.mValues = actions;
        return parameter;
    }
}

AllowListCommand::AllowListCommand(ServerNetworkHandler &handler)
        : Command("allowlist", "commands.allowlist.description",
                  "/allowlist <on|off|list|reload> | /allowlist <add|remove> <player>", {"whitelist"}),
          mHandler(handler) {}

std::vector<CommandOverloadData> AllowListCommand::getOverloads() const {
    CommandOverloadData toggle;
    toggle.mParameters.push_back(makeActionParameter("AllowListAction", {"on", "off", "list", "reload"}));

    CommandOverloadData edit;
    edit.mParameters.push_back(makeActionParameter("AllowListEditAction", {"add", "remove"}));
    edit.mParameters.push_back(makePlayerParameter("player", mHandler.getPlayerNames()));

    return {toggle, edit};
}

bool AllowListCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string action = StringUtil::toLowerCase(arguments[0]);
    AllowList &allowList = mHandler.getAllowList();

    if (action == "on") {
        mHandler.setAllowListEnabled(true);
        sender.sendMessage("Allowlist is now on");
        return true;
    }

    if (action == "off") {
        mHandler.setAllowListEnabled(false);
        sender.sendMessage("Allowlist is now off");
        return true;
    }

    if (action == "list") {
        std::string names;
        for (const std::string &name: allowList.getNames()) {
            if (!names.empty())
                names += ", ";
            names += name;
        }

        sender.sendMessage("Allowlist (" + std::to_string(allowList.getNames().size()) + "): " + names);
        return true;
    }

    if (action == "reload") {
        allowList.reload();
        mHandler.kickNotAllowListedPlayers();
        sender.sendMessage("Allowlist reloaded");
        return true;
    }

    if ((action == "add" || action == "remove") && arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (action == "add") {
        if (!allowList.add(arguments[1])) {
            sender.sendMessage(arguments[1] + " is already in the allowlist");
            return false;
        }

        sender.sendMessage("Added " + arguments[1] + " to the allowlist");
        return true;
    }

    if (action == "remove") {
        if (!allowList.remove(arguments[1])) {
            sender.sendMessage(arguments[1] + " is not in the allowlist");
            return false;
        }

        mHandler.kickNotAllowListedPlayers();
        sender.sendMessage("Removed " + arguments[1] + " from the allowlist");
        return true;
    }

    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}
