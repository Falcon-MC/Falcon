#include "Server/Commands/MusicCommand.h"

#include "Core/NBT/Tag.h"
#include "World/Level/MusicEvents.h"

#include <cstdio>

namespace {
    const float DEFAULT_VOLUME = 1.0f;
    const float MIN_TRACK_VOLUME = 0.01f;
    const float MAX_VOLUME = 1.0f;
    const float MAX_FADE_SECONDS = 10.0f;

    bool parseFloat(const std::string &value, float &out) {
        return Command::parseCoordinate(value, 0.0f, out) && value[0] != '~';
    }

    // the game prints both numbers with two decimals
    std::string twoDecimals(float value) {
        char text[32];
        std::snprintf(text, sizeof(text), "%.2f", value);
        return text;
    }

    bool inRange(CommandOrigin &sender, float value, float min, float max) {
        if (value < min) {
            sender.sendTranslation("commands.generic.num.tooSmall", {twoDecimals(value), twoDecimals(min)});
            return false;
        }
        if (value > max) {
            sender.sendTranslation("commands.generic.num.tooBig", {twoDecimals(value), twoDecimals(max)});
            return false;
        }
        return true;
    }
}

MusicCommand::MusicCommand(ServerNetworkHandler &handler)
        : Command("music", "commands.music.description",
                  "/music <queue|play> <trackName> [volume] [fadeSeconds] [play_once|loop] | /music stop "
                  "[fadeSeconds] | /music volume <volume>"),
          mHandler(handler) {}

std::vector<CommandOverloadData> MusicCommand::getOverloads() const {
    CommandOverloadData track;
    track.mParameters.push_back(makeEnumParameter("mode", "MusicTrackMode", {"queue", "play"}));
    track.mParameters.push_back(makeTypedParameter("trackName", CommandParamType::String));
    track.mParameters.push_back(makeTypedParameter("volume", CommandParamType::Float, true));
    track.mParameters.push_back(makeTypedParameter("fadeSeconds", CommandParamType::Float, true));
    track.mParameters.push_back(makeEnumParameter("repeatMode", "MusicRepeatMode", {"play_once", "loop"}, true));

    CommandOverloadData stop;
    stop.mParameters.push_back(makeEnumParameter("mode", "MusicStopMode", {"stop"}));
    stop.mParameters.push_back(makeTypedParameter("fadeSeconds", CommandParamType::Float, true));

    CommandOverloadData volume;
    volume.mParameters.push_back(makeEnumParameter("mode", "MusicVolumeMode", {"volume"}));
    volume.mParameters.push_back(makeTypedParameter("volume", CommandParamType::Float));
    return {track, stop, volume};
}

bool MusicCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const std::string mode = arguments.empty() ? std::string() : arguments[0];
    const bool queueing = mode == "queue";
    const bool playing = mode == "play";
    const bool stopping = mode == "stop";
    const bool setting = mode == "volume";
    const bool shaped = ((queueing || playing) && arguments.size() >= 2 && arguments.size() <= 5)
                        || (stopping && arguments.size() <= 2) || (setting && arguments.size() == 2);
    if (!shaped) {
        sendUsageError(sender);
        return false;
    }

    float volume = DEFAULT_VOLUME;
    float fade = 0.0f;
    bool loop = false;
    bool valid = true;
    if (queueing || playing) {
        valid = (arguments.size() < 3 || parseFloat(arguments[2], volume))
                && (arguments.size() < 4 || parseFloat(arguments[3], fade))
                && (arguments.size() < 5 || arguments[4] == "play_once" || arguments[4] == "loop");
        loop = arguments.size() >= 5 && arguments[4] == "loop";
    } else if (stopping) {
        valid = arguments.size() < 2 || parseFloat(arguments[1], fade);
    } else {
        valid = parseFloat(arguments[1], volume);
    }
    if (!valid) {
        sendUsageError(sender);
        return false;
    }

    if ((queueing || playing) && arguments[1].empty()) {
        sender.sendTranslation("commands.music.failure.emptyTrackName", {});
        return false;
    }
    // volume alone may go down to silence; a track starts at least audible
    if (!inRange(sender, volume, setting ? 0.0f : MIN_TRACK_VOLUME, MAX_VOLUME)
        || !inRange(sender, fade, 0.0f, MAX_FADE_SECONDS))
        return false;

    if (queueing || playing) {
        MusicEvents::send(mHandler, nullptr, queueing ? MusicEvents::QUEUE : MusicEvents::PLAY,
                          MusicEvents::trackTag(arguments[1], volume, fade, loop));
        sender.sendTranslation(queueing ? "commands.music.success.queueAction" : "commands.music.success.playAction",
                               {arguments[1]});
        return true;
    }

    Tag data = Tag::ofCompound();
    if (stopping) {
        data.putFloat("fadeSeconds", fade);
        MusicEvents::send(mHandler, nullptr, MusicEvents::STOP, data);
        sender.sendTranslation("commands.music.success.stopAction", {});
        return true;
    }
    data.putFloat("volume", volume);
    MusicEvents::send(mHandler, nullptr, MusicEvents::VOLUME, data);
    sender.sendTranslation("commands.music.success.volumeAction", {twoDecimals(volume)});
    return true;
}
