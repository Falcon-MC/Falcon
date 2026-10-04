#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Command/AboutCommand.h"
#include "Command/AllowListCommand.h"
#include "Command/BanCommand.h"
#include "Command/BanIpCommand.h"
#include "Command/BanListCommand.h"
#include "Command/CameraCommand.h"
#include "Command/ClearCommand.h"
#include "Command/CommandOrigin.h"
#include "Command/DamageCommand.h"
#include "Command/DayLockCommand.h"
#include "Command/DefaultGameModeCommand.h"
#include "Command/DeopCommand.h"
#include "Command/DifficultyCommand.h"
#include "Command/EffectCommand.h"
#include "Command/EnchantCommand.h"
#include "Command/ExecuteCommand.h"
#include "Command/FillCommand.h"
#include "Command/GameModeCommand.h"
#include "Command/GameRuleCommand.h"
#include "Command/GiveCommand.h"
#include "Command/KickCommand.h"
#include "Command/KillCommand.h"
#include "Command/ListCommand.h"
#include "Command/LocateCommand.h"
#include "Command/MeCommand.h"
#include "Command/OpCommand.h"
#include "Command/PardonCommand.h"
#include "Command/PardonIpCommand.h"
#include "Command/ParticleCommand.h"
#include "Command/PlaySoundCommand.h"
#include "Command/ProfilerCommand.h"
#include "Command/SaveCommand.h"
#include "Command/SayCommand.h"
#include "Command/SeedCommand.h"
#include "Command/SetBlockCommand.h"
#include "Command/SetMaxPlayersCommand.h"
#include "Command/SetWorldSpawnCommand.h"
#include "Command/SpawnPointCommand.h"
#include "Command/StatusCommand.h"
#include "Command/StopCommand.h"
#include "Command/StopSoundCommand.h"
#include "Command/SummonCommand.h"
#include "Command/TagCommand.h"
#include "Command/TickingAreaCommand.h"
#include "Command/TeleportCommand.h"
#include "Command/TellCommand.h"
#include "Command/TellRawCommand.h"
#include "Command/TestForBlockCommand.h"
#include "Command/TestForBlocksCommand.h"
#include "Command/TestForCommand.h"
#include "Command/TimeCommand.h"
#include "Command/WorldCommand.h"
#include "Command/TitleCommand.h"
#include "Command/TransferCommand.h"
#include "Command/WeatherCommand.h"
#include "Command/XpCommand.h"
#include "Core/Event/GameEvents.h"
#include "Network/Handler/MovementHandler.h"
#include "Plugin/PluginManager.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Protocol/Packets/SetPlayerGameTypePacket.h"
#include "Protocol/Types/StartGameTypes.h"

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

    struct SelectorFilter {
        bool mValid = false;
        char mKind = 0;
        std::vector<std::pair<std::string, bool>> mTypes;
        std::string mName;
        bool mHasName = false;
        bool mNameNegated = false;
        size_t mLimit = 0;
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

    /**
     * Parses "@k" and "@k[key=value,...]". Only type, name and c are understood: an unknown argument makes
     * the selector invalid rather than silently widening it to every target.
     */
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
            size_t comma = body.find(',', start);
            if (comma == std::string::npos)
                comma = body.size();

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

            if (key == "type") {
                filter.mTypes.emplace_back(normalizeTypeId(value), negated);
            } else if (key == "name") {
                filter.mHasName = true;
                filter.mNameNegated = negated;
                filter.mName = value;
            } else if (key == "c") {
                if (negated || value.empty() || value.size() > 9 || value.find_first_not_of("0123456789") != std::string::npos)
                    return filter;
                filter.mLimit = (size_t) std::stoul(value);
                if (filter.mLimit == 0)
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

std::vector<ServerPlayer *> ServerNetworkHandler::resolveTargets(CommandOrigin &sender,
                                                                 const std::string &selector) {
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
        if (self != nullptr && matchesName(filter, self->getName()))
            targets.push_back(self);
        return targets;
    }

    if (filter.mKind != 'a' && filter.mKind != 'e' && filter.mKind != 'r')
        return targets;

    const ServerPlayer *senderPlayer = sender.asPlayer();
    const uint32_t worldId = senderPlayer != nullptr ? getWorldFor(*senderPlayer).getId()
                                                     : getWorldOf(sender.getLevel()).getId();
    const size_t limit = filter.mKind == 'r' && filter.mLimit == 0 ? 1 : filter.mLimit;

    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !matchesName(filter, player.getName()))
            continue;
        if (filter.mKind == 'e' && player.getWorldId() != worldId)
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

    const ServerPlayer *senderPlayer = sender.asPlayer();
    const uint32_t worldId = senderPlayer != nullptr ? getWorldFor(*senderPlayer).getId()
                                                     : getWorldOf(sender.getLevel()).getId();

    for (auto &entry: mActors) {
        ServerActor *actor = entry.second.get();
        if (actor == nullptr || actor->isDead() || actor->getWorldId() != worldId)
            continue;
        if (!matchesType(filter, actor->getTypeId()) || !matchesName(filter, actor->getName()))
            continue;

        targets.push_back(actor);
        if (filter.mLimit != 0 && targets.size() >= filter.mLimit)
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
    }

    player.setGameType(gameMode);
    player.setHungerEnabled(gameMode == (int32_t) GameType::Survival || gameMode == (int32_t) GameType::Adventure);

    const bool mayFly = gameMode == (int32_t) GameType::Creative || gameMode == (int32_t) GameType::Spectator;
    if (!mayFly && player.isFlying()) {
        player.setFlying(false);
        player.setOnGround(MovementHandler::checkGroundState(getLevelFor(player), player.getPosition()));
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
                                             const std::string &key,
                                             const std::vector<std::string> &parameters) {
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
