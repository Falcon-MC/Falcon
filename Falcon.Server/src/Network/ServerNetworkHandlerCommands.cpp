#include "Network/ServerNetworkHandler.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Server/Commands/AboutCommand.h"
#include "Server/Commands/AllowListCommand.h"
#include "Server/Commands/BanCommand.h"
#include "Server/Commands/BanIpCommand.h"
#include "Server/Commands/BanListCommand.h"
#include "Server/Commands/CameraCommand.h"
#include "Server/Commands/ClearCommand.h"
#include "Server/Commands/CommandOrigin.h"
#include "Server/Commands/DamageCommand.h"
#include "Server/Commands/DayLockCommand.h"
#include "Server/Commands/DefaultGameModeCommand.h"
#include "Server/Commands/DeopCommand.h"
#include "Server/Commands/DifficultyCommand.h"
#include "Server/Commands/EffectCommand.h"
#include "Server/Commands/EnchantCommand.h"
#include "Server/Commands/ExecuteCommand.h"
#include "Server/Commands/FillCommand.h"
#include "Server/Commands/GameModeCommand.h"
#include "Server/Commands/GameRuleCommand.h"
#include "Server/Commands/GiveCommand.h"
#include "Server/Commands/KickCommand.h"
#include "Server/Commands/KillCommand.h"
#include "Server/Commands/ListCommand.h"
#include "Server/Commands/LocateCommand.h"
#include "Server/Commands/MeCommand.h"
#include "Server/Commands/OpCommand.h"
#include "Server/Commands/PardonCommand.h"
#include "Server/Commands/PardonIpCommand.h"
#include "Server/Commands/ParticleCommand.h"
#include "Server/Commands/PlaySoundCommand.h"
#include "Server/Commands/ProfilerCommand.h"
#include "Server/Commands/SaveCommand.h"
#include "Server/Commands/ScoreboardCommand.h"
#include "Server/Commands/SayCommand.h"
#include "Server/Commands/ScriptEventCommand.h"
#include "Server/Commands/SeedCommand.h"
#include "Server/Commands/SetBlockCommand.h"
#include "Server/Commands/SetMaxPlayersCommand.h"
#include "Server/Commands/SetWorldSpawnCommand.h"
#include "Server/Commands/SpawnPointCommand.h"
#include "Server/Commands/AimAssistCommand.h"
#include "Server/Commands/CameraShakeCommand.h"
#include "Server/Commands/DebugHudCommand.h"
#include "Server/Commands/StatusCommand.h"
#include "Server/Commands/StopCommand.h"
#include "Server/Commands/StopSoundCommand.h"
#include "Server/Commands/SummonCommand.h"
#include "Server/Commands/TagCommand.h"
#include "Server/Commands/TickingAreaCommand.h"
#include "Server/Commands/TeleportCommand.h"
#include "Server/Commands/TellCommand.h"
#include "Server/Commands/TellRawCommand.h"
#include "Server/Commands/TestForBlockCommand.h"
#include "Server/Commands/TestForBlocksCommand.h"
#include "Server/Commands/TestForCommand.h"
#include "Server/Commands/TimeCommand.h"
#include "Server/Commands/WorldCommand.h"
#include "Server/Commands/TitleCommand.h"
#include "Server/Commands/TransferCommand.h"
#include "Server/Commands/WeatherCommand.h"
#include "Server/Commands/XpCommand.h"
#include "Core/Event/GameEvents.h"
#include "Network/Handler/MovementHandler.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Protocol/Packets/SetPlayerGameTypePacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <climits>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
    std::string toLowerCopy(const std::string &value) {
        std::string lowered = value;
        for (char &character: lowered) {
            if (character >= 'A' && character <= 'Z')
                character = (char) (character - 'A' + 'a');
        }
        return lowered;
    }

    struct ScoreFilter {
        std::string mObjective;
        int32_t mMin = INT32_MIN;
        int32_t mMax = INT32_MAX;
        bool mNegated = false;
    };

    struct SelectorFilter {
        bool mValid = false;
        char mKind = 0;
        std::vector<std::pair<std::string, bool>> mTypes;
        std::string mName;
        bool mHasName = false;
        bool mNameNegated = false;
        size_t mLimit = 0;
        std::vector<ScoreFilter> mScores;
    };

    std::string normalizeTypeId(const std::string &type) {
        const std::string lowered = toLowerCopy(type);
        return lowered.find(':') == std::string::npos ? "minecraft:" + lowered : lowered;
    }

    std::string trimCopy(const std::string &value) {
        size_t begin = 0;
        size_t end = value.size();
        while (begin < end && (value[begin] == ' ' || value[begin] == '\t'))
            ++begin;
        while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t'))
            --end;
        return value.substr(begin, end - begin);
    }

    bool parseScoreBound(const std::string &value, int32_t &out) {
        if (value.empty() || value.size() > 11)
            return false;
        char *end = nullptr;
        const long long parsed = std::strtoll(value.c_str(), &end, 10);
        if (*end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX)
            return false;
        out = (int32_t) parsed;
        return true;
    }

    // {objective=5,other=1..3,last=!..0}: a range is n, n.., ..n or n..m, both ends included
    bool parseScores(const std::string &value, std::vector<ScoreFilter> &out) {
        if (value.size() < 2 || value.front() != '{' || value.back() != '}')
            return false;
        const std::string body = value.substr(1, value.size() - 2);
        size_t start = 0;
        while (start < body.size()) {
            size_t comma = body.find(',', start);
            if (comma == std::string::npos)
                comma = body.size();
            const std::string entry = trimCopy(body.substr(start, comma - start));
            start = comma + 1;
            const size_t equals = entry.find('=');
            if (equals == std::string::npos)
                return false;
            ScoreFilter filter;
            filter.mObjective = trimCopy(entry.substr(0, equals));
            std::string range = trimCopy(entry.substr(equals + 1));
            if (!range.empty() && range[0] == '!') {
                filter.mNegated = true;
                range = trimCopy(range.substr(1));
            }
            const size_t dots = range.find("..");
            if (filter.mObjective.empty() || range.empty())
                return false;
            if (dots == std::string::npos) {
                if (!parseScoreBound(range, filter.mMin))
                    return false;
                filter.mMax = filter.mMin;
            } else {
                const std::string low = range.substr(0, dots);
                const std::string high = range.substr(dots + 2);
                if ((low.empty() && high.empty()) || (!low.empty() && !parseScoreBound(low, filter.mMin))
                    || (!high.empty() && !parseScoreBound(high, filter.mMax)))
                    return false;
            }
            out.push_back(filter);
        }
        return !out.empty();
    }

    // sub_1455ECBD0: a target without a score fails, negated or not
    bool matchesScores(const SelectorFilter &filter, Scoreboard &scoreboard, const Actor &actor) {
        if (filter.mScores.empty())
            return true;
        const ScoreboardIdentity *identity = scoreboard.findActor(actor.getUniqueId());
        for (const ScoreFilter &score: filter.mScores) {
            const ScoreboardObjective *objective = scoreboard.getObjective(score.mObjective);
            const int32_t *value = identity == nullptr || objective == nullptr ? nullptr
                                                                               : objective->find(identity->mId);
            if (value == nullptr)
                return false;
            if ((*value >= score.mMin && *value <= score.mMax) == score.mNegated)
                return false;
        }
        return true;
    }

    // "@k" and "@k[key=value,...]"; only type, name, c and scores: an unknown argument invalidates the selector
    // instead of widening it to every target
    SelectorFilter parseSelector(const std::string &selector) {
        SelectorFilter filter;
        if (selector.size() < 2 || selector[0] != '@')
            return filter;

        filter.mKind = selector[1];
        if (selector.size() == 2) {
            filter.mValid = true;
            return filter;
        }

        if (selector[2] != '[' || selector.back() != ']')
            return filter;

        const std::string body = selector.substr(3, selector.size() - 4);
        size_t start = 0;
        while (start <= body.size()) {
            size_t comma = start;
            bool quoted = false;
            bool escaped = false;
            int braces = 0;
            for (; comma < body.size(); ++comma) {
                const char character = body[comma];
                if (escaped)
                    escaped = false;
                else if (quoted && character == '\\')
                    escaped = true;
                else if (character == '"')
                    quoted = !quoted;
                else if (!quoted && character == '{')
                    ++braces;
                else if (!quoted && character == '}' && braces > 0)
                    --braces;
                else if (!quoted && braces == 0 && character == ',')
                    break;
            }
            if (quoted || escaped || braces != 0)
                return filter;

            const std::string entry = trimCopy(body.substr(start, comma - start));
            start = comma + 1;
            if (entry.empty()) {
                if (comma == body.size())
                    break;
                continue;
            }

            const size_t equals = entry.find('=');
            if (equals == std::string::npos)
                return filter;

            const std::string key = toLowerCopy(trimCopy(entry.substr(0, equals)));
            std::string value = trimCopy(entry.substr(equals + 1));
            bool negated = false;
            if (!value.empty() && value[0] == '!') {
                negated = true;
                value = trimCopy(value.substr(1));
            }
            if (!value.empty() && value[0] == '"') {
                if (value.size() < 2 || value.back() != '"')
                    return filter;
                std::string decoded;
                for (size_t i = 1; i + 1 < value.size(); ++i) {
                    if (value[i] == '\\') {
                        if (++i + 1 >= value.size())
                            return filter;
                    } else if (value[i] == '"') {
                        return filter;
                    }
                    decoded.push_back(value[i]);
                }
                value = std::move(decoded);
            }

            if (key == "type") {
                filter.mTypes.emplace_back(normalizeTypeId(value), negated);
            } else if (key == "name") {
                filter.mHasName = true;
                filter.mNameNegated = negated;
                filter.mName = value;
            } else if (key == "c") {
                if (negated || value.empty() || value.size() > 9
                    || value.find_first_not_of("0123456789") != std::string::npos)
                    return filter;
                filter.mLimit = (size_t) std::stoul(value);
                if (filter.mLimit == 0)
                    return filter;
            } else if (key == "scores") {
                if (negated || !parseScores(value, filter.mScores))
                    return filter;
            } else {
                return filter;
            }

            if (comma == body.size())
                break;
        }

        filter.mValid = true;
        return filter;
    }

    bool matchesType(const SelectorFilter &filter, const std::string &typeId) {
        const std::string normalized = normalizeTypeId(typeId);
        for (const auto &type: filter.mTypes) {
            if ((normalized == type.first) == type.second)
                return false;
        }
        return true;
    }

    bool matchesName(const SelectorFilter &filter, const std::string &name) {
        if (!filter.mHasName)
            return true;
        return (name == filter.mName) != filter.mNameNegated;
    }
}

void ServerNetworkHandler::_registerCommands() {
    mCommands.registerCommand(std::make_shared<GameModeCommand>(*this));
    mCommands.registerCommand(std::make_shared<OpCommand>(*this));
    mCommands.registerCommand(std::make_shared<DeopCommand>(*this));
    mCommands.registerCommand(std::make_shared<GiveCommand>(*this));
    mCommands.registerCommand(std::make_shared<EnchantCommand>(*this));
    mCommands.registerCommand(std::make_shared<EffectCommand>(*this));
    mCommands.registerCommand(std::make_shared<KillCommand>(*this));
    mCommands.registerCommand(std::make_shared<SetBlockCommand>(*this));
    mCommands.registerCommand(std::make_shared<FillCommand>(*this));
    mCommands.registerCommand(std::make_shared<SummonCommand>(*this));
    mCommands.registerCommand(std::make_shared<ExecuteCommand>(*this));
    mCommands.registerCommand(std::make_shared<XpCommand>(*this));
    mCommands.registerCommand(std::make_shared<DifficultyCommand>(*this));
    mCommands.registerCommand(std::make_shared<ListCommand>(*this));
    mCommands.registerCommand(std::make_shared<TitleCommand>(*this, false));
    mCommands.registerCommand(std::make_shared<TitleCommand>(*this, true));
    mCommands.registerCommand(std::make_shared<SpawnPointCommand>(*this, false));
    mCommands.registerCommand(std::make_shared<SpawnPointCommand>(*this, true));
    mCommands.registerCommand(std::make_shared<SetWorldSpawnCommand>(*this));
    mCommands.registerCommand(std::make_shared<SayCommand>(*this));
    mCommands.registerCommand(std::make_shared<ScriptEventCommand>(*this));
    mCommands.registerCommand(std::make_shared<TellCommand>(*this));
    mCommands.registerCommand(std::make_shared<MeCommand>(*this));
    mCommands.registerCommand(std::make_shared<TellRawCommand>(*this));
    mCommands.registerCommand(std::make_shared<TimeCommand>(*this));
    mCommands.registerCommand(std::make_shared<WorldCommand>(*this));
    mCommands.registerCommand(std::make_shared<WeatherCommand>(*this));
    mCommands.registerCommand(std::make_shared<GameRuleCommand>(*this));
    mCommands.registerCommand(std::make_shared<ProfilerCommand>(*this));
    mCommands.registerCommand(std::make_shared<CameraCommand>(*this));
    mCommands.registerCommand(std::make_shared<ClearCommand>(*this));
    mCommands.registerCommand(std::make_shared<LocateCommand>(*this));
    mCommands.registerCommand(std::make_shared<AboutCommand>(*this));
    mCommands.registerCommand(std::make_shared<AllowListCommand>(*this));
    mCommands.registerCommand(std::make_shared<TeleportCommand>(*this));
    mCommands.registerCommand(std::make_shared<KickCommand>(*this));
    mCommands.registerCommand(std::make_shared<BanCommand>(*this));
    mCommands.registerCommand(std::make_shared<PardonCommand>(*this));
    mCommands.registerCommand(std::make_shared<BanIpCommand>(*this));
    mCommands.registerCommand(std::make_shared<PardonIpCommand>(*this));
    mCommands.registerCommand(std::make_shared<BanListCommand>(*this));
    mCommands.registerCommand(std::make_shared<SeedCommand>(*this));
    mCommands.registerCommand(std::make_shared<DefaultGameModeCommand>(*this));
    mCommands.registerCommand(std::make_shared<SetMaxPlayersCommand>(*this));
    mCommands.registerCommand(std::make_shared<StopCommand>(*this));
    mCommands.registerCommand(std::make_shared<SaveCommand>(*this, SaveCommand::Mode::Save));
    mCommands.registerCommand(std::make_shared<SaveCommand>(*this, SaveCommand::Mode::On));
    mCommands.registerCommand(std::make_shared<SaveCommand>(*this, SaveCommand::Mode::Off));
    mCommands.registerCommand(std::make_shared<StatusCommand>(*this));
    mCommands.registerCommand(std::make_shared<DebugHudCommand>(*this));
    mCommands.registerCommand(std::make_shared<CameraShakeCommand>(*this));
    mCommands.registerCommand(std::make_shared<AimAssistCommand>(*this));
    mCommands.registerCommand(std::make_shared<TransferCommand>(*this));
    mCommands.registerCommand(std::make_shared<PlaySoundCommand>(*this));
    mCommands.registerCommand(std::make_shared<StopSoundCommand>(*this));
    mCommands.registerCommand(std::make_shared<ParticleCommand>(*this));
    mCommands.registerCommand(std::make_shared<DamageCommand>(*this));
    mCommands.registerCommand(std::make_shared<DayLockCommand>(*this));
    mCommands.registerCommand(std::make_shared<TestForCommand>(*this));
    mCommands.registerCommand(std::make_shared<TestForBlockCommand>());
    mCommands.registerCommand(std::make_shared<TestForBlocksCommand>());
    mCommands.registerCommand(std::make_shared<TagCommand>(*this));
    mCommands.registerCommand(std::make_shared<ScoreboardCommand>(*this));
    mCommands.registerCommand(std::make_shared<TickingAreaCommand>(*this));
}

ServerPlayer *ServerNetworkHandler::getPlayerByName(const std::string &name) {
    for (auto &entry: mPlayers) {
        if (entry.second.getName() == name)
            return &entry.second;
    }

    const std::string lowered = toLowerCopy(name);

    for (auto &entry: mPlayers) {
        if (toLowerCopy(entry.second.getName()) == lowered)
            return &entry.second;
    }

    ServerPlayer *partial = nullptr;
    for (auto &entry: mPlayers) {
        const std::string candidate = toLowerCopy(entry.second.getName());
        if (candidate.compare(0, lowered.size(), lowered) != 0)
            continue;

        if (partial != nullptr)
            return nullptr;

        partial = &entry.second;
    }

    return partial;
}

std::vector<std::string> ServerNetworkHandler::getPlayerNames() const {
    std::vector<std::string> names;
    names.reserve(mPlayers.size());

    for (const auto &entry: mPlayers) {
        if (!entry.second.getName().empty())
            names.push_back(entry.second.getName());
    }

    return names;
}

std::vector<ServerPlayer *> ServerNetworkHandler::resolveTargets(CommandOrigin &sender, const std::string &selector) {
    std::vector<ServerPlayer *> targets;

    if (selector.empty() || selector[0] != '@') {
        ServerPlayer *named = getPlayerByName(selector);
        if (named != nullptr)
            targets.push_back(named);
        return targets;
    }

    const SelectorFilter filter = parseSelector(selector);
    if (!filter.mValid || !matchesType(filter, "minecraft:player"))
        return targets;

    if (filter.mKind == 's' || filter.mKind == 'p') {
        ServerPlayer *self = sender.asPlayer();
        if (self != nullptr && matchesName(filter, self->getName()) && matchesScores(filter, mScoreboard, *self))
            targets.push_back(self);
        return targets;
    }

    if (filter.mKind != 'a' && filter.mKind != 'e' && filter.mKind != 'r')
        return targets;

    const Level *contextLevel = sender.getLevel();
    if (contextLevel == nullptr)
        contextLevel = &getLevel();
    const size_t limit = filter.mKind == 'r' && filter.mLimit == 0 ? 1 : filter.mLimit;

    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !matchesName(filter, player.getName())
            || !matchesScores(filter, mScoreboard, player))
            continue;
        if (filter.mKind == 'e' && !player.isIn(*contextLevel))
            continue;

        targets.push_back(&player);
        if (limit != 0 && targets.size() >= limit)
            break;
    }

    return targets;
}

std::vector<ServerActor *> ServerNetworkHandler::resolveActorTargets(CommandOrigin &sender,
                                                                     const std::string &selector) {
    std::vector<ServerActor *> targets;

    const SelectorFilter filter = parseSelector(selector);
    if (!filter.mValid || filter.mKind != 'e')
        return targets;

    const Level *contextLevel = sender.getLevel();
    if (contextLevel == nullptr)
        contextLevel = &getLevel();
    size_t limit = filter.mLimit;
    if (limit != 0) {
        const size_t players = resolveTargets(sender, selector).size();
        if (players >= limit)
            return targets;
        limit -= players;
    }

    for (auto &entry: mActors) {
        ServerActor *actor = entry.second.get();
        if (actor == nullptr || actor->isDead() || !actor->isIn(*contextLevel))
            continue;
        if (!matchesType(filter, actor->getTypeId()) || !matchesName(filter, actor->getName())
            || !matchesScores(filter, mScoreboard, *actor))
            continue;

        targets.push_back(actor);
        if (limit != 0 && targets.size() >= limit)
            break;
    }

    return targets;
}

void ServerNetworkHandler::setPlayerGameMode(ServerPlayer &player, int gameMode) {
    const int32_t previousGameMode = player.getGameType();

    if (previousGameMode != gameMode) {
        PluginEvent gameModeEvent;
        gameModeEvent.mType = FALCON_EVENT_PLAYER_GAME_MODE_CHANGE;
        gameModeEvent.mCancellable = true;
        gameModeEvent.mPlayer = &player;
        gameModeEvent.mGameMode = gameMode;
        gameModeEvent.mPreviousGameMode = previousGameMode;
        mPluginManager->dispatch(gameModeEvent);
        if (gameModeEvent.mCancelled)
            return;
        if (mScriptEngine.beforePlayerGameModeChange(player, previousGameMode, gameMode))
            return;
    }

    player.setGameType(gameMode);
    player.setHungerEnabled(gameMode == (int32_t) GameType::Survival || gameMode == (int32_t) GameType::Adventure);

    const bool mayFly = gameMode == (int32_t) GameType::Creative || gameMode == (int32_t) GameType::Spectator;
    if (!mayFly && player.isFlying()) {
        player.setFlying(false);
        player.setOnGround(MovementHandler::checkGroundState(getLevelFor(player), player.getPosition(), &player));
    }

    if (gameMode == (int32_t) GameType::Spectator) {
        player.setFlying(true);
        player.setOnGround(false);
    }

    _sendAbilities(player);

    SetPlayerGameTypePacket packet;
    packet.mGamemode = gameMode;
    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);

    player.resetFallDistance();

    if (previousGameMode != gameMode) {
        PlayerGameModeChangeAfterEvent event(player, previousGameMode, gameMode);
        mEventBus.after().mPlayerGameModeChange.emit(event);
        mScriptEngine.onPlayerGameModeChange(player, previousGameMode, gameMode);
    }
}

void ServerNetworkHandler::sendCommandOutput(ServerPlayer &player, const CommandOriginData &origin,
                                             const std::string &message) {
    CommandOutputPacket output;
    output.mOrigin = origin;
    output.mType = CommandOutputType::AllOutput;
    output.mSuccessCount = 1;

    CommandOutputMessage line;
    line.mInternal = false;
    line.mMessageId = message;
    output.mMessages.push_back(line);

    mNetworkHandler->send(player.getNetworkIdentifier(), output, mCodecContext);
}

void ServerNetworkHandler::sendCommandOutput(ServerPlayer &player, const CommandOriginData &origin,
                                             const std::string &key, const std::vector<std::string> &parameters) {
    CommandOutputPacket output;
    output.mOrigin = origin;
    output.mType = CommandOutputType::AllOutput;
    output.mSuccessCount = 1;

    CommandOutputMessage line;
    line.mInternal = false;
    line.mMessageId = key;
    line.mParameters = parameters;
    output.mMessages.push_back(std::move(line));

    mNetworkHandler->send(player.getNetworkIdentifier(), output, mCodecContext);
}
