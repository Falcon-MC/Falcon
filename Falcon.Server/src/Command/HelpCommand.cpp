#include "Command/HelpCommand.h"

#include "Command/CommandMap.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cstdlib>

namespace {
    const int COMMANDS_PER_PAGE = 7;
}

HelpCommand::HelpCommand(ServerNetworkHandler &handler)
        : Command("help", "commands.help.description", "/help [page|command]", {"?"}), mHandler(handler) {}

std::vector<CommandOverloadData> HelpCommand::getOverloads() const {
    CommandOverloadData byPage;
    byPage.mParameters.push_back(makeTypedParameter("page", CommandParamType::Int, true));

    CommandOverloadData byCommand;
    byCommand.mParameters.push_back(makeTypedParameter("command", CommandParamType::String));

    return {byPage, byCommand};
}

bool HelpCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    std::vector<Command *> commands;
    for (Command *command: mHandler.getCommands().getCommands()) {
        if ((int) sender.getCommandPermission() >= (int) command->getRequiredPermission())
            commands.push_back(command);
    }

    std::sort(commands.begin(), commands.end(), [](const Command *left, const Command *right) {
        return left->getName() < right->getName();
    });
    commands.erase(std::unique(commands.begin(), commands.end()), commands.end());

    int page = 1;
    if (!arguments.empty()) {
        char *end = nullptr;
        const long requested = std::strtol(arguments[0].c_str(), &end, 10);

        if (end == arguments[0].c_str() || *end != '\0') {
            Command *command = mHandler.getCommands().getCommand(arguments[0]);
            if (command == nullptr) {
                sender.sendTranslation("commands.generic.unknown", {arguments[0]});
                return false;
            }

            sender.sendTranslation("%" + command->getDescription(), {});
            sender.sendTranslation("commands.generic.usage", {command->getUsage()});
            return true;
        }

        page = (int) requested;
    }

    const int pageCount = std::max(1, ((int) commands.size() + COMMANDS_PER_PAGE - 1) / COMMANDS_PER_PAGE);
    page = std::min(std::max(page, 1), pageCount);

    sender.sendTranslation("commands.help.header", {std::to_string(page), std::to_string(pageCount)});

    const size_t first = (size_t) (page - 1) * COMMANDS_PER_PAGE;
    for (size_t index = first; index < commands.size() && index < first + COMMANDS_PER_PAGE; ++index)
        sender.sendTranslation("/" + commands[index]->getName() + " - %" + commands[index]->getDescription(), {});

    sender.sendTranslation("commands.help.footer", {});
    return true;
}
