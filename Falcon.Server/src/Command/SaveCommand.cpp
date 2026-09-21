#include "Command/SaveCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const char *nameOf(SaveCommand::Mode mode) {
        switch (mode) {
            case SaveCommand::Mode::On:
                return "save-on";
            case SaveCommand::Mode::Off:
                return "save-off";
            default:
                return "save";
        }
    }
}

SaveCommand::SaveCommand(ServerNetworkHandler &handler, Mode mode)
        : Command(nameOf(mode), "commands.save.description",
                  mode == Mode::Save ? "/save [hold|resume|query]" : std::string("/") + nameOf(mode)),
          mHandler(handler), mMode(mode) {}

std::vector<CommandOverloadData> SaveCommand::getOverloads() const {
    CommandOverloadData overload;
    if (mMode == Mode::Save)
        overload.mParameters.push_back(makeEnumParameter("mode", "SaveMode", {"hold", "resume", "query"}, true));
    return {overload};
}

bool SaveCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (mMode == Mode::On || mMode == Mode::Off) {
        setAutoSave(sender, mMode == Mode::On);
        return true;
    }

    const std::string action = arguments.empty() ? std::string() : arguments[0];

    if (action == "hold") {
        setAutoSave(sender, false);
        return true;
    }

    if (action == "resume") {
        setAutoSave(sender, true);
        return true;
    }

    if (action == "query") {
        sender.sendTranslation(mHandler.isAutoSaveEnabled() ? "commands.save.enabled" : "commands.save.disabled", {});
        return true;
    }

    if (!action.empty()) {
        sender.sendTranslation("commands.generic.parameter.invalid", {action});
        return false;
    }

    sender.sendTranslation("commands.save.start", {});
    mHandler.autoSave();
    sender.sendTranslation("commands.save.success", {});
    return true;
}

void SaveCommand::setAutoSave(CommandOrigin &sender, bool enabled) {
    mHandler.setAutoSaveEnabled(enabled);
    sender.sendTranslation(enabled ? "commands.save.enabled" : "commands.save.disabled", {});
}
