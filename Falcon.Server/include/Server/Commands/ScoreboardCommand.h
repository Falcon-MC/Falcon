#pragma once

#include "Server/Commands/Command.h"

#include <cstdint>

class ServerNetworkHandler;

class ScoreboardCommand : public Command {
public:
    explicit ScoreboardCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    // identity ids a participant argument names: "*", a selector or a name; create makes the missing ones
    static std::vector<int64_t> participants(ServerNetworkHandler &handler, CommandOrigin &sender,
                                             const std::string &argument, bool create);

private:
    bool _objectives(CommandOrigin &sender, const std::vector<std::string> &arguments);

    bool _players(CommandOrigin &sender, const std::vector<std::string> &arguments);

    bool _list(CommandOrigin &sender, const std::vector<std::string> &arguments);

    bool _change(CommandOrigin &sender, const std::vector<std::string> &arguments);

    bool _reset(CommandOrigin &sender, const std::vector<std::string> &arguments);

    bool _test(CommandOrigin &sender, const std::vector<std::string> &arguments);

    bool _random(CommandOrigin &sender, const std::vector<std::string> &arguments);

    bool _operation(CommandOrigin &sender, const std::vector<std::string> &arguments);

    ServerNetworkHandler &mHandler;
};
