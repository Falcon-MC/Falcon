#pragma once

#include "Server/Commands/Command.h"

class CommandMap;

class HelpCommand : public Command {
public:
    explicit HelpCommand(CommandMap &commands);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override {
        return CommandPermission::Any;
    }

    std::vector<CommandOverloadData> getOverloads() const override;

    // one line per overload, as the game prints it: "/name <param: type> [param: type]"
    static std::vector<std::string> usageLines(const Command &command);

private:
    CommandMap &mCommands;
};
