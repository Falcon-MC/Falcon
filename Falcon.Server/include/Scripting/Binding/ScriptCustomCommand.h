#pragma once

#include "Server/Commands/Command.h"

#include <string>
#include <vector>

#include <quickjs.h>

class ScriptApi;

// a CustomCommandParameter: mType is its CustomCommandParamType value
struct ScriptCommandParameter {
    std::string mName;
    std::string mType;
    // the command enum an Enum parameter takes its values from
    std::string mEnum;
    // the beta enumName set mEnum, not the parameter name
    bool mNamedEnum = false;
    bool mOptional = false;
};

// a command CustomCommandRegistry.registerCommand added to the server's command map
class ScriptCustomCommand : public Command {
public:
    // callback: taken, freed with the command
    ScriptCustomCommand(ScriptApi &api, const std::string &name, const std::string &description,
                        CommandPermission permission, bool cheatsRequired,
                        std::vector<ScriptCommandParameter> parameters, std::vector<std::string> aliases,
                        JSValue callback);

    ~ScriptCustomCommand() override;

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override {
        return mPermission;
    }

    std::vector<CommandOverloadData> getOverloads() const override;

    bool requiresCheats() const {
        return mCheatsRequired;
    }

    const std::vector<ScriptCommandParameter> &getParameters() const {
        return mParameters;
    }

    JSValueConst getCallback() const {
        return mCallback;
    }

private:
    ScriptApi &mApi;
    CommandPermission mPermission;
    bool mCheatsRequired;
    std::vector<ScriptCommandParameter> mParameters;
    JSValue mCallback;
};
