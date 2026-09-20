#include "Command/OpCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"

OpCommand::OpCommand(ServerNetworkHandler &handler)
        : Command("op", "commands.op.description", "/op <player>"), mHandler(handler) {}

bool OpCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    mHandler.getOpList().addOp(arguments[0]);

    ServerPlayer *target = mHandler.getPlayerByName(arguments[0]);
    if (target != nullptr)
        mHandler.setPlayerOp(*target, true);

    sender.sendTranslation("commands.op.success", {arguments[0]});
    return true;
}

std::vector<CommandOverloadData> OpCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("player"));
    return {overload};
}
