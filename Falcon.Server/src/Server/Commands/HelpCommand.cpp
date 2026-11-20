#include "Server/Commands/HelpCommand.h"

#include "Server/Commands/CommandMap.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>

namespace {
    // the game's help shows seven lines a page
    const int LINES_PER_PAGE = 7;

    const char *typeName(CommandParamType type) {
        switch (type) {
            case CommandParamType::Int:
                return "int";
            case CommandParamType::Float:
                return "float";
            case CommandParamType::Value:
                return "value";
            case CommandParamType::WildcardInt:
                return "wildcard int";
            case CommandParamType::Operator:
                return "operator";
            case CommandParamType::CompareOperator:
                return "compare operator";
            case CommandParamType::Target:
            case CommandParamType::WildcardTarget:
                return "target";
            case CommandParamType::Filepath:
                return "filepath";
            case CommandParamType::IntegerRange:
                return "integer range";
            case CommandParamType::EquipmentSlots:
                return "equipment slots";
            case CommandParamType::String:
                return "string";
            case CommandParamType::BlockPosition:
            case CommandParamType::Position:
                return "x y z";
            case CommandParamType::Message:
                return "message";
            case CommandParamType::RawText:
                return "text";
            case CommandParamType::Json:
                return "json";
            case CommandParamType::BlockStates:
                return "block states";
            case CommandParamType::Command:
                return "command";
        }
        return "unknown";
    }

    std::string parameterUsage(const CommandParamData &parameter) {
        // a mandatory enum of one value is a subcommand: the game writes the word alone
        if (parameter.mHasEnumData && parameter.mEnumData.mValues.size() == 1 && !parameter.mOptional)
            return parameter.mEnumData.mValues.front();

        const std::string type = parameter.mHasEnumData ? parameter.mEnumData.mName
                                 : parameter.mHasType    ? typeName(parameter.mType)
                                                         : std::string();
        return (parameter.mOptional ? "[" : "<") + parameter.mName + ": " + type + (parameter.mOptional ? "]" : ">");
    }

    bool readPage(const std::string &text, int &page) {
        char *end = nullptr;
        errno = 0;
        const long value = std::strtol(text.c_str(), &end, 10);
        if (text.empty() || *end != '\0' || errno == ERANGE)
            return false;
        page = (int) std::clamp<long>(value, INT32_MIN, INT32_MAX);
        return true;
    }

    // a description with no space is a translation key the client resolves
    std::string describe(const std::string &description) {
        return description.find(' ') == std::string::npos ? "§e%" + description : "§e" + description;
    }
}

HelpCommand::HelpCommand(CommandMap &commands)
        : Command("help", "commands.help.description", "/help [page: int]", {"?"}), mCommands(commands) {}

std::vector<CommandOverloadData> HelpCommand::getOverloads() const {
    std::vector<std::string> names;
    for (const Command *command: mCommands.getCommands())
        names.push_back(command->getName());
    std::sort(names.begin(), names.end());

    CommandOverloadData byName;
    byName.mParameters.push_back(makeEnumParameter("command", "CommandName", names));
    CommandOverloadData byPage;
    byPage.mParameters.push_back(makeTypedParameter("page", CommandParamType::Int, true));
    return {byName, byPage};
}

std::vector<std::string> HelpCommand::usageLines(const Command &command) {
    std::vector<std::string> lines;
    for (const CommandOverloadData &overload: command.getOverloads()) {
        std::string line = "/" + command.getName();
        for (const CommandParamData &parameter: overload.mParameters)
            line += " " + parameterUsage(parameter);
        lines.push_back(line);
    }
    std::sort(lines.begin(), lines.end());
    return lines;
}

bool HelpCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    int page = 1;
    if (arguments.empty() || readPage(arguments[0], page)) {
        std::vector<Command *> commands = mCommands.getCommands();
        std::sort(commands.begin(), commands.end(),
                  [](const Command *left, const Command *right) { return left->getName() < right->getName(); });
        std::vector<std::string> lines;
        for (const Command *command: commands) {
            if (!sender.canExecuteCommand(*command))
                continue;
            for (std::string &line: usageLines(*command))
                lines.push_back(std::move(line));
        }

        const int count = (int) lines.size();
        const int pages = std::max(1, (count + LINES_PER_PAGE - 1) / LINES_PER_PAGE);
        page = std::clamp(page, 1, pages);
        std::string body;
        for (int index = (page - 1) * LINES_PER_PAGE; index < std::min(count, page * LINES_PER_PAGE); ++index) {
            if (!body.empty())
                body += "\n";
            body += lines[(size_t) index];
        }

        sender.sendTranslation("§2%commands.help.header", {std::to_string(page), std::to_string(pages)});
        sender.sendMessage(body);
        sender.sendTranslation("§2%commands.help.footer", {});
        return true;
    }

    const Command *command = mCommands.getCommand(arguments[0]);
    if (command == nullptr || !sender.canExecuteCommand(*command)) {
        sender.sendTranslation("commands.generic.unknown", {arguments[0]});
        return false;
    }

    if (command->getAliases().empty()) {
        sender.sendMessage("§e" + command->getName() + ":");
    } else {
        std::string aliases;
        for (const std::string &alias: command->getAliases())
            aliases += (aliases.empty() ? "" : ", ") + alias;
        sender.sendTranslation("commands.help.command.aliases", {"§e" + command->getName(), aliases});
    }
    sender.sendTranslation(describe(command->getDescription()), {});
    sender.sendTranslation("commands.generic.usage.noparam", {});
    std::string usage;
    for (const std::string &line: usageLines(*command))
        usage += (usage.empty() ? "- " : "\n- ") + line;
    sender.sendMessage(usage);
    return true;
}
