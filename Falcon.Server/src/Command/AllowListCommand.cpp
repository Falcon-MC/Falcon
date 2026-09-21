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
    edit.mParameters.push_back(makePlayerParameter("player"));

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
        sender.sendLocalized("falcon.commands.allowlist.enabled");
        return true;
    }

    if (action == "off") {
        mHandler.setAllowListEnabled(false);
        sender.sendLocalized("falcon.commands.allowlist.disabled");
        return true;
    }

    if (action == "list") {
        std::string names;
        for (const std::string &name: allowList.getNames()) {
            if (!names.empty())
                names += ", ";
            names += name;
        }

        sender.sendLocalized("falcon.commands.allowlist.list",
                             {std::to_string(allowList.getNames().size()), names});
        return true;
    }

    if (action == "reload") {
        allowList.reload();
        mHandler.kickNotAllowListedPlayers();
        sender.sendLocalized("falcon.commands.allowlist.reloaded");
        return true;
    }

    if ((action == "add" || action == "remove") && arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (action == "add") {
        if (!allowList.add(arguments[1])) {
            sender.sendLocalized("falcon.commands.allowlist.add.failed", {arguments[1]});
            return false;
        }

        sender.sendLocalized("falcon.commands.allowlist.add.success", {arguments[1]});
        return true;
    }

    if (action == "remove") {
        if (!allowList.remove(arguments[1])) {
            sender.sendLocalized("falcon.commands.allowlist.remove.failed", {arguments[1]});
            return false;
        }

        mHandler.kickNotAllowListedPlayers();
        sender.sendLocalized("falcon.commands.allowlist.remove.success", {arguments[1]});
        return true;
    }

    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}
