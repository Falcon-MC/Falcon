#include "Server/Commands/CommandMap.h"
#include "Server/Commands/HelpCommand.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    class RecordingOrigin : public CommandOrigin {
    public:
        explicit RecordingOrigin(CommandPermission permission) : mPermission(permission) {}

        std::string getSenderName() const override {
            return "Tester";
        }

        bool isPlayer() const override {
            return false;
        }

        ServerPlayer *asPlayer() override {
            return nullptr;
        }

        void sendMessage(const std::string &message) override {
            mLines.push_back(message);
        }

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            std::string line = "{" + key;
            for (const std::string &parameter: parameters)
                line += "|" + parameter;
            mLines.push_back(line + "}");
        }

        CommandPermission getCommandPermission() const override {
            return mPermission;
        }

        CommandPermission mPermission;
        std::vector<std::string> mLines;
    };

    class FakeCommand : public Command {
    public:
        FakeCommand(const std::string &name, CommandPermission permission, std::vector<CommandOverloadData> overloads,
                    const std::vector<std::string> &aliases = {})
                : Command(name, "Does " + name, "/" + name, aliases), mPermission(permission),
                  mOverloads(std::move(overloads)) {}

        bool execute(CommandOrigin &, const std::vector<std::string> &) override {
            return true;
        }

        CommandPermission getRequiredPermission() const override {
            return mPermission;
        }

        std::vector<CommandOverloadData> getOverloads() const override {
            return mOverloads;
        }

    private:
        CommandPermission mPermission;
        std::vector<CommandOverloadData> mOverloads;
    };

    CommandOverloadData overload(std::vector<CommandParamData> parameters) {
        CommandOverloadData data;
        data.mParameters = std::move(parameters);
        return data;
    }
}

int main() {
    try {
        CommandMap commands;
        commands.registerCommand(std::make_shared<HelpCommand>(commands));
        // a subcommand word, an enum, a position and an optional target, as the game writes them
        commands.registerCommand(std::make_shared<FakeCommand>(
                "zone", CommandPermission::Any,
                std::vector<CommandOverloadData>{
                        overload({Command::makeEnumParameter("action", "ZoneSet", {"set"}),
                                  Command::makeTypedParameter("from", CommandParamType::Position),
                                  Command::makePlayerParameter("player")}),
                        overload({Command::makeEnumParameter("mode", "ZoneMode", {"on", "off"}),
                                  Command::makeTypedParameter("who", CommandParamType::Target, true)})},
                std::vector<std::string>{"z"}));
        for (const char *name: {"a1", "a2", "a3", "a4", "a5", "a6"})
            commands.registerCommand(std::make_shared<FakeCommand>(name, CommandPermission::Any,
                                                                   std::vector<CommandOverloadData>{overload({})}));
        commands.registerCommand(std::make_shared<FakeCommand>("secret", CommandPermission::Admin,
                                                               std::vector<CommandOverloadData>{overload({})}));

        const std::vector<std::string> zone = HelpCommand::usageLines(*commands.getCommand("zone"));
        require(zone.size() == 2 && zone[0] == "/zone <mode: ZoneMode> [who: target]"
                        && zone[1] == "/zone set <from: x y z> <player: target>",
                "the usage lines are not written as the game writes them: " + zone[0]);

        // 6 + 2 zone + 2 help lines the member may use, the admin one left out: 10 lines on 2 pages of 7
        RecordingOrigin member(CommandPermission::Any);
        require(commands.dispatch(member, "/help"), "/help failed");
        require(member.mLines.size() == 3 && member.mLines[0] == "{§2%commands.help.header|1|2}"
                        && member.mLines[2] == "{§2%commands.help.footer}",
                "the first page was not framed by the header and the footer");
        require(member.mLines[1].find("/a1") == 0 && member.mLines[1].find("secret") == std::string::npos,
                "the first page did not start with the first command, or showed one out of reach");
        member.mLines.clear();
        require(commands.dispatch(member, "/? 9"), "/? past the last page failed");
        require(member.mLines[0] == "{§2%commands.help.header|2|2}"
                        && member.mLines[1]
                                   == "/help [page: int]\n/zone <mode: ZoneMode> [who: target]\n"
                                      "/zone set <from: x y z> <player: target>",
                "a page past the last was not the last one: " + member.mLines[1]);

        member.mLines.clear();
        require(commands.dispatch(member, "/help z"), "/help of an alias failed");
        require(member.mLines.size() == 4 && member.mLines[0] == "{commands.help.command.aliases|§ezone|z}"
                        && member.mLines[1] == "{§eDoes zone}" && member.mLines[2] == "{commands.generic.usage.noparam}"
                        && member.mLines[3]
                                   == "- /zone <mode: ZoneMode> [who: target]\n- /zone set <from: x y z> <player: target>",
                "the help of one command is not the game's");

        member.mLines.clear();
        require(!commands.dispatch(member, "/help secret"), "the help of a command out of reach was given");
        require(member.mLines.size() == 1 && member.mLines[0] == "{commands.generic.unknown|secret}",
                "a command out of reach was not unknown");
        std::cout << "help command checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
