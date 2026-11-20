#include "Server/Commands/FunctionCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "Server/Functions/FunctionManager.h"

FunctionCommand::FunctionCommand(ServerNetworkHandler &handler)
        : Command("function", "commands.function.description", "/function <name: filepath>"), mHandler(handler) {}

std::vector<CommandOverloadData> FunctionCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makeTypedParameter("name", CommandParamType::Filepath));
    return {overload};
}

bool FunctionCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string &name = arguments[0];
    if (const char invalid = FunctionManager::invalidCharacter(name)) {
        sender.sendTranslation("commands.function.invalidCharacters", {name, std::string(1, invalid)});
        return false;
    }

    FunctionManager &functions = mHandler.getFunctions();
    const FunctionManager::Function *function = functions.find(name);
    if (function == nullptr) {
        sender.sendTranslation("commands.function.functionNameNotFound", {name});
        return false;
    }
    if (function->mFailed) {
        sender.sendTranslation("commands.generic.exception", {});
        return false;
    }

    // called from a function: its commands run next, in the same chain
    if (functions.isRunning()) {
        functions.push(*function, sender);
        return true;
    }

    const int executed = functions.run(*function, sender, mHandler.getCommands(), mHandler.getCommandChainLimit());
    if (executed > 0)
        sender.sendTranslation("commands.function.success", {std::to_string(executed)});
    return true;
}
