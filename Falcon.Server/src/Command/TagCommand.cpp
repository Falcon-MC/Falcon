#include "Command/TagCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

TagCommand::TagCommand(ServerNetworkHandler &handler)
        : Command("tag", "commands.tag.description", "/tag <targets> <add|remove|list> [name]"), mHandler(handler) {}

std::vector<CommandOverloadData> TagCommand::getOverloads() const {
    CommandOverloadData change;
    change.mParameters = {makePlayerParameter("targets"), makeEnumParameter("action", "TagChangeAction", {"add", "remove"}),
                          makeTypedParameter("name", CommandParamType::String)};

    CommandOverloadData list;
    list.mParameters = {makePlayerParameter("targets"), makeEnumParameter("action", "TagListAction", {"list"})};

    return {change, list};
}

bool TagCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    const std::string &action = arguments[1];

    if (action == "list") {
        std::string tags;
        size_t count = 0;
        for (const ServerPlayer *target: targets) {
            for (const std::string &tag: target->getTags()) {
                if (!tags.empty())
                    tags += ", ";
                tags += tag;
                ++count;
            }
        }

        if (targets.size() == 1) {
            if (count == 0)
                sender.sendTranslation("commands.tag.list.single.empty", {targets[0]->getName()});
            else
                sender.sendTranslation("commands.tag.list.single.success",
                                       {targets[0]->getName(), std::to_string(count), tags});
        } else if (count == 0) {
            sender.sendTranslation("commands.tag.list.multiple.empty", {std::to_string(targets.size())});
        } else {
            sender.sendTranslation("commands.tag.list.multiple.success",
                                   {std::to_string(targets.size()), std::to_string(count), tags});
        }
        return true;
    }

    if ((action != "add" && action != "remove") || arguments.size() < 3) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string &name = arguments[2];
    const bool adding = action == "add";

    size_t changed = 0;
    std::string lastName;
    for (ServerPlayer *target: targets) {
        if (adding ? target->addTag(name) : target->removeTag(name)) {
            ++changed;
            lastName = target->getName();
        }
    }

    if (changed == 0) {
        sender.sendTranslation(adding ? "commands.tag.add.failed" : "commands.tag.remove.failed", {});
        return false;
    }

    if (changed == 1)
        sender.sendTranslation(adding ? "commands.tag.add.success.single" : "commands.tag.remove.success.single",
                               {name, lastName});
    else
        sender.sendTranslation(adding ? "commands.tag.add.success.multiple" : "commands.tag.remove.success.multiple",
                               {name, std::to_string(changed)});
    return true;
}
