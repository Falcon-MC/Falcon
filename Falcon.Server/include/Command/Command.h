#pragma once

#include "Command/CommandOrigin.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/AdventureSettingData.h"
#include "Protocol/Types/CommandData.h"

#include <string>
#include <vector>

class Server;

class Command {
public:
    Command(const std::string &name, const std::string &description, const std::string &usage,
            const std::vector<std::string> &aliases = {});

    virtual ~Command() = default;

    virtual bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) = 0;

    virtual CommandPermission getRequiredPermission() const { return CommandPermission::GameDirectors; }

    virtual std::vector<CommandOverloadData> getOverloads() const;

    virtual size_t getRawArgumentIndex() const { return (size_t) -1; }

    static CommandParamData makePlayerParameter(const std::string &name);

    static CommandParamData makeTypedParameter(const std::string &name, CommandParamType type, bool optional = false);

    static CommandParamData makeEnumParameter(const std::string &name, const std::string &enumName,
                                              const std::vector<std::string> &values, bool optional = false);

    static std::string joinArguments(const std::vector<std::string> &arguments, size_t first);

    static bool parseCoordinate(const std::string &value, float origin, float &out);

    static bool parseBlockCoordinate(const std::string &value, int32_t origin, int32_t &out);

    static bool parseBlockPosition(const std::vector<std::string> &arguments, size_t first,
                                   const Vector3i &origin, Vector3i &out);

    const std::string &getName() const { return mName; }

    const std::string &getDescription() const { return mDescription; }

    const std::string &getUsage() const { return mUsage; }

    const std::vector<std::string> &getAliases() const { return mAliases; }

protected:
    std::string mName;
    std::string mDescription;
    std::string mUsage;
    std::vector<std::string> mAliases;
};
