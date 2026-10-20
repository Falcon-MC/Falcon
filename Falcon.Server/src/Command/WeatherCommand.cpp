#include "Command/WeatherCommand.h"

#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const int32_t DEFAULT_DURATION_TICKS = 6000;
    const int32_t MAX_DURATION_SECONDS = 1000000;
}

WeatherCommand::WeatherCommand(ServerNetworkHandler &handler)
        : Command("weather", "commands.weather.description", "/weather <clear|rain|thunder> [duration]"),
          mHandler(handler) {}

CommandPermission WeatherCommand::getRequiredPermission() const {
    return CommandPermission::GameDirectors;
}

std::vector<CommandOverloadData> WeatherCommand::getOverloads() const {
    CommandParamData typeParameter;
    typeParameter.mName = "type";
    typeParameter.mHasEnumData = true;
    typeParameter.mEnumData.mName = "WeatherType";
    typeParameter.mEnumData.mIsSoft = false;
    typeParameter.mEnumData.mValues = {"clear", "rain", "thunder"};

    CommandParamData durationParameter;
    durationParameter.mName = "duration";
    durationParameter.mOptional = true;

    CommandOverloadData overload;
    overload.mParameters.push_back(typeParameter);
    overload.mParameters.push_back(durationParameter);

    return {overload};
}

bool WeatherCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string &type = arguments[0];
    if (type != "clear" && type != "rain" && type != "thunder") {
        sender.sendTranslation("commands.generic.parameter.invalid", {type});
        return false;
    }

    int32_t durationTicks = DEFAULT_DURATION_TICKS;
    if (arguments.size() > 1) {
        try {
            const long seconds = std::stol(arguments[1]);
            if (seconds <= 0 || seconds > MAX_DURATION_SECONDS) {
                sender.sendTranslation("commands.generic.num.invalid", {arguments[1]});
                return false;
            }
            durationTicks = (int32_t) (seconds * 20);
        } catch (const std::exception &) {
            sender.sendTranslation("commands.generic.num.invalid", {arguments[1]});
            return false;
        }
    }

    World &world = mHandler.getWorldOf(sender.getLevel());
    Level &level = world.getOverworld();

    if (type == "clear") {
        mHandler.setThundering(world, false);
        mHandler.setRaining(world, false);
        level.setRainTime(durationTicks);
        level.setThunderTime(durationTicks);
        mHandler.broadcastWeather(world);
        sender.sendTranslation("commands.weather.clear", {});
        return true;
    }

    if (type == "rain") {
        mHandler.setThundering(world, false);
        mHandler.setRaining(world, true);
        level.setRainTime(durationTicks);
        mHandler.broadcastWeather(world);
        sender.sendTranslation("commands.weather.rain", {});
        return true;
    }

    mHandler.setRaining(world, true);
    mHandler.setThundering(world, true);
    level.setRainTime(durationTicks);
    level.setThunderTime(durationTicks);
    mHandler.broadcastWeather(world);
    sender.sendTranslation("commands.weather.thunder", {});
    return true;
}
