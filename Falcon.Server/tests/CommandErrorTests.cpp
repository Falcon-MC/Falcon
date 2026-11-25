#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Protocol/Packets/CommandRequestPacket.h"
#include "Server/Commands/CommandMap.h"
#include "Server/Commands/CommandOrigin.h"
#include "Server/Commands/CommandSyntax.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // the console: runs every command, records what it says
    class RecordingOrigin : public CommandOrigin {
    public:
        explicit RecordingOrigin(Level *level) : mLevel(level) {}

        std::vector<std::string> mOutput;

        std::string getSenderName() const override {
            return "command-error-test";
        }

        bool isPlayer() const override {
            return false;
        }

        ServerPlayer *asPlayer() override {
            return nullptr;
        }

        void sendMessage(const std::string &message) override {
            mOutput.push_back(message);
        }

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            std::string line = key;
            for (const std::string &parameter: parameters)
                line += "|" + parameter;
            mOutput.push_back(line);
        }

        void sendError(const std::string &message) override {
            mOutput.push_back("error:" + message);
        }

        CommandPermission getCommandPermission() const override {
            return CommandPermission::Internal;
        }

        bool canManageServer() const override {
            return true;
        }

        bool canUseCommandsWithoutCheats() const override {
            return true;
        }

        Level *getLevel() override {
            return mLevel;
        }

    private:
        Level *mLevel;
    };

    std::vector<std::string> run(ServerNetworkHandler &server, Level &level, const std::string &line, bool *result = nullptr) {
        RecordingOrigin origin(&level);
        const bool success = server.getCommands().dispatch(origin, line);
        if (result != nullptr)
            *result = success;
        return origin.mOutput;
    }

    std::string only(const std::vector<std::string> &output) {
        return output.size() == 1 ? output[0] : "<" + std::to_string(output.size()) + " messages>";
    }

    void syntaxWindows() {
        const CommandSyntaxError middle = CommandSyntax::around("/give @s diamond abc", 17, 20);
        require(middle.mBefore == "s diamond " && middle.mToken == "abc" && middle.mAfter.empty(),
                "the token was not shown with the ten characters before it");
        const CommandSyntaxError start = CommandSyntax::around("/tp x y", 4, 5);
        require(start.mBefore == "/tp " && start.mToken == "x" && start.mAfter == " y", "a short line was cut");
        const CommandSyntaxError far = CommandSyntax::around("/a 123456789012345 b 123456789012345", 19, 20);
        require(far.mBefore == "789012345 " && far.mToken == "b" && far.mAfter == " 123456789",
                "more than ten characters were shown on a side");
        const CommandSyntaxError end = CommandSyntax::around("/give @s", 8, 8);
        require(end.mBefore == "/give @s" && end.mToken.empty() && end.mAfter.empty(), "a missing token was misplaced");
    }

    void syntaxErrors(ServerNetworkHandler &server, Level &level) {
        bool result = true;
        require(only(run(server, level, "/summon cow 1 2 x", &result)) == "commands.generic.syntax|n cow 1 2 |x|"
                        && !result,
                "a bad coordinate did not give the game's syntax error: " + only(run(server, level, "/summon cow 1 2 x")));
        require(only(run(server, level, "time set")) == "commands.generic.syntax|time set||",
                "a missing argument did not point at the end: " + only(run(server, level, "time set")));
        require(only(run(server, level, "/difficulty impossible")) == "commands.generic.syntax|ifficulty |impossible|",
                "a bad enum value was not pointed at: " + only(run(server, level, "/difficulty impossible")));
        require(only(run(server, level, "/setblock 1 2")) == "commands.generic.syntax|tblock 1 2||",
                "a missing coordinate was not pointed at: " + only(run(server, level, "/setblock 1 2")));
        // coordinates written without spaces are read as the command reads them: the error is the mode
        require(only(run(server, level, "/fill ~~~ ~2~2~2 stone badmode"))
                        == "commands.generic.syntax|2~2 stone |badmode|",
                "unspaced coordinates moved the syntax error: "
                        + only(run(server, level, "/fill ~~~ ~2~2~2 stone badmode")));
        require(only(run(server, level, "/nosuchcommand")) == "commands.generic.unknown|nosuchcommand",
                "an unknown command lost its message");
        // a well formed line that fails for another reason keeps that reason
        const std::string own = only(run(server, level, "/give nobody diamond", &result));
        require(!result && own.rfind("commands.generic.syntax", 0) != 0 && own.rfind("<", 0) != 0,
                "a valid line lost its own error: " + own);
    }

    // every failing command says why; prints the ones that do not
    void everyFailureSaysSomething(ServerNetworkHandler &server, Level &level) {
        static const char *const JUNK[] = {"", "@@@", "1.5.5 zz", "~ ~ ~ ~ ~ ~ ~ ~", "@a 99999999999999999999"};
        std::vector<std::string> silent;
        for (Command *command: server.getCommands().getCommands()) {
            const std::string &name = command->getName();
            // these stop or reload the server, or wait on the network
            if (name == "stop" || name == "reload" || name == "transfer" || name == "save" || name == "gametest"
                || name == "profiler" || name == "script")
                continue;
            for (const char *junk: JUNK) {
                const std::string line = "/" + name + (junk[0] == '\0' ? "" : " ") + junk;
                bool result = true;
                const std::vector<std::string> output = run(server, level, line, &result);
                if (!result && output.empty())
                    silent.push_back(line);
            }
        }
        for (const std::string &line: silent)
            std::cerr << "silent failure: " << line << '\n';
        require(silent.empty(), std::to_string(silent.size()) + " failing command lines said nothing");
    }

    void playerOutput(ServerNetworkHandler &server, Level &level) {
        std::vector<CommandOutputPacket> outputs;
        server.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &, std::string &bytes) {
            ReadOnlyBinaryStream header(bytes);
            unsigned char sender = 0, recipient = 0;
            if (Packet::peekId(header, sender, recipient) == MinecraftPacketIds::CommandOutput) {
                ReadOnlyBinaryStream body(bytes);
                CommandOutputPacket packet;
                packet.readHeader(body);
                packet.read(body, server.getCodecContext());
                outputs.push_back(std::move(packet));
            }
            return false;
        });
        const NetworkIdentifier id("command-errors", 4210);
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4210).first->second;
        player.moveToLevel(level);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setOp(true);

        const auto request = [&](const std::string &line) {
            outputs.clear();
            CommandRequestPacket packet;
            packet.mCommand = line;
            packet.handle(id, server);
            require(outputs.size() == 1, "\"" + line + "\" did not answer with one command output");
            return outputs.front();
        };

        const CommandOutputPacket failed = request("/give @s diamond abc");
        require(failed.mSuccessCount == 0 && failed.mMessages.size() == 1 && !failed.mMessages[0].mInternal
                        && failed.mMessages[0].mMessageId == "commands.generic.syntax",
                "a failed command was not shown as an error");
        const CommandOutputPacket succeeded = request("/time set 1000");
        require(succeeded.mSuccessCount == 1 && !succeeded.mMessages.empty() && succeeded.mMessages[0].mInternal,
                "a successful command was shown as an error");

        // sendcommandfeedback false: nothing for a command that works, the error still for one that fails
        level.getGameRules().setFromString("sendcommandfeedback", "false");
        outputs.clear();
        CommandRequestPacket quiet;
        quiet.mCommand = "/time set 2000";
        quiet.handle(id, server);
        require(outputs.empty() && level.getDayTime() == 2000, "sendcommandfeedback false still showed a success");
        const CommandOutputPacket stillFailed = request("/summon cow 1 2 x");
        require(stillFailed.mSuccessCount == 0 && stillFailed.mMessages.size() == 1,
                "sendcommandfeedback false hid an error");
        level.getGameRules().setFromString("sendcommandfeedback", "true");
        server.getNetworkHandler().setOutboundFilter(nullptr);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
            / ("falcon-command-errors-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        ServerNetworkHandler server("Command errors", "", 1);
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);

        syntaxWindows();
        syntaxErrors(server, level);
        playerOutput(server, level);
        everyFailureSaysSomething(server, level);
        std::cout << "command error checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
