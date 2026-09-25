#include "Network/Handler/ServerNetworkHandler.h"

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
#include "Command/TeleportCommand.h"
#include "Command/TellCommand.h"
#include "Command/TellRawCommand.h"
#include "Command/TestForBlockCommand.h"
#include "Command/TestForBlocksCommand.h"
#include "Command/TestForCommand.h"
#include "Command/TimeCommand.h"
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

    if (selector == "@a" || selector == "@e") {
        for (auto &entry: mPlayers) {
            if (entry.second.isSpawned())
                targets.push_back(&entry.second);
        }
        return targets;
    }

    if (selector == "@s" || selector == "@p") {
        ServerPlayer *self = sender.asPlayer();
        if (self != nullptr)
            targets.push_back(self);
        return targets;
    }

    if (selector == "@r") {
        for (auto &entry: mPlayers) {
            if (entry.second.isSpawned()) {
                targets.push_back(&entry.second);
                break;
            }
        }
        return targets;
    }

    ServerPlayer *named = getPlayerByName(selector);
    if (named != nullptr)
        targets.push_back(named);

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
