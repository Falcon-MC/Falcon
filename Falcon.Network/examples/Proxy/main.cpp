#include "Network/Auth/MinecraftAuthentication.h"
#include "Network/Auth/XboxLiveConfig.h"
#include "Network/Client/ClientNetworkSystem.h"
#include "Network/Crypto/KeyPair.h"
#include "Network/Server/BedrockListener.h"
#include "Protocol/Packets/TextPacket.h"

#include <chrono>
#include <cstdio>
#include <exception>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

    const int PROTOCOL_VERSION = 2223;
    const char *GAME_VERSION = "1.26.60";
    const unsigned int JOIN_TIMEOUT_MS = 30000;

    void printText(const char *direction, const BedrockConnection &connection, const std::string &payload) {
        MinecraftPacketIds id;
        if (!BedrockConnection::peekPacketId(payload, id) || id != MinecraftPacketIds::Text)
            return;

        const std::shared_ptr<TextPacket> text = std::dynamic_pointer_cast<TextPacket>(connection.decode(payload));
        if (text == nullptr)
            return;

        std::string line = std::string("[") + direction + "] ";
        if (!text->mSourceName.empty())
            line += "<" + text->mSourceName + "> ";
        line += text->mMessage;
        for (const std::string &parameter: text->mParameters)
            line += " | " + parameter;

        std::printf("%s\n", line.c_str());
        std::fflush(stdout);
    }

    void forward(BedrockConnection &from, BedrockConnection &to, const char *direction) {
        std::string payload;
        while (from.readRaw(payload)) {
            printText(direction, from, payload);
            to.sendRaw(payload);
        }

        to.disconnect(from.getDisconnectReason());
    }

    void handlePlayer(IncomingConnection incoming, std::string host, unsigned short port,
                      MinecraftAuthentication *authentication, std::string username) {
        BedrockConnection &player = *incoming.mConnection;
        std::printf("%s joined, connecting to %s:%u\n", incoming.mIdentity.mDisplayName.c_str(), host.c_str(), port);

        ClientConnectionSettings settings;
        settings.mHost = host;
        settings.mPort = port;
        settings.mProtocolVersion = incoming.mProtocolVersion;
        settings.mGameVersion = incoming.mGameVersion.empty() ? GAME_VERSION : incoming.mGameVersion;
        settings.mIdentity.mDisplayName = username.empty() ? incoming.mIdentity.mDisplayName : username;
        settings.mAuthentication = authentication;
        settings.mDeferSpawn = true;

        ClientConnectionResult result = ClientNetworkSystem::dial(settings);
        if (result.mConnection == nullptr) {
            std::printf("Could not connect: %s\n", result.mError.c_str());
            player.disconnect(result.mError);
            return;
        }

        BedrockConnection &server = *result.mConnection;
        player.setCodecContext(&server.getCodecContext());

        std::string error;
        if (server.getStartGame() == nullptr) {
            std::printf("The server did not send StartGame\n");
            player.disconnect("The server did not send StartGame");
            server.disconnect("");
            return;
        }

        if (!player.startGame(*server.getStartGame(), server.getItemRegistry().get(), server.getChunkRadius(),
                              JOIN_TIMEOUT_MS, nullptr, error)
            || !server.spawn(JOIN_TIMEOUT_MS, nullptr, error)) {
            std::printf("Could not spawn: %s\n", error.c_str());
            player.disconnect(error);
            server.disconnect(error);
            return;
        }

        std::thread serverToPlayer(forward, std::ref(server), std::ref(player), "server -> player");
        forward(player, server, "player -> server");
        serverToPlayer.join();

        std::printf("%s left\n", incoming.mIdentity.mDisplayName.c_str());
    }

    void runPlayer(IncomingConnection incoming, std::string host, unsigned short port,
                   MinecraftAuthentication *authentication, std::string username) {
        try {
            handlePlayer(std::move(incoming), std::move(host), port, authentication, std::move(username));
        } catch (const std::exception &exception) {
            std::printf("Player session failed: %s\n", exception.what());
        } catch (...) {
            std::printf("Player session failed\n");
        }
        std::fflush(stdout);
    }

    void waitSeconds(int seconds) {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
    }

    std::string ask(const std::string &question) {
        std::string answer;
        while (answer.empty()) {
            std::cout << question << std::flush;
            if (!std::getline(std::cin, answer)) {
                std::cin.clear();
                waitSeconds(1);
                continue;
            }

            while (!answer.empty() && (answer.back() == ' ' || answer.back() == '\r'))
                answer.pop_back();
        }
        return answer;
    }

    bool askYesNo(const std::string &question) {
        for (;;) {
            const std::string answer = ask(question);
            if (answer == "y" || answer == "Y")
                return true;
            if (answer == "n" || answer == "N")
                return false;

            std::cout << "Please answer y or n" << std::endl;
        }
    }

    bool parseAddress(const std::string &input, std::string &host, unsigned short &port) {
        const size_t colon = input.rfind(':');
        host = input.substr(0, colon);
        port = colon == std::string::npos ? 19132 : (unsigned short) std::atoi(input.substr(colon + 1).c_str());
        return !host.empty() && port != 0;
    }

    std::unique_ptr<MinecraftAuthentication> logIn() {
        for (;;) {
            std::unique_ptr<MinecraftAuthentication> authentication(
                    new MinecraftAuthentication(XboxLiveConfig::android(), "token.json", GAME_VERSION));
            authentication->getLiveAuthentication().setDeviceCodeCallback(
                    [](const std::string &verificationUri, const std::string &userCode) {
                        std::cout << "Open " << verificationUri << " and enter the code " << userCode << std::endl;
                    });

            std::cout << "Logging in to Microsoft..." << std::endl;
            const std::shared_ptr<KeyPair> key = KeyPair::generate();
            MinecraftAuthenticationResult result;
            std::string error;
            if (key != nullptr && authentication->authenticate(*key, false, result, error)) {
                std::cout << "Logged in as " << result.mDisplayName << std::endl;
                return authentication;
            }

            std::cout << "Microsoft login failed: " << error << std::endl;
            if (!askYesNo("Try again? (y/n): "))
                return nullptr;
        }
    }

    void run() {
        std::string host;
        unsigned short port = 0;
        while (!parseAddress(ask("Remote server (ip:port): "), host, port))
            std::cout << "Invalid address" << std::endl;

        std::unique_ptr<MinecraftAuthentication> authentication;
        std::string username;

        if (askYesNo("Use a Microsoft account? (y/n): "))
            authentication = logIn();

        if (authentication == nullptr)
            username = ask("Username: ");

        ListenerSettings settings;
        settings.mServerName = "FalconProxy";
        settings.mSubName = host + ":" + std::to_string(port);
        settings.mProtocolVersion = PROTOCOL_VERSION;
        settings.mGameVersion = GAME_VERSION;
        settings.mNetherNet = true;
        settings.mAuthentication = authentication.get();

        for (;;) {
            BedrockListener listener;
            std::string error;
            if (!listener.listen(settings, error)) {
                std::cout << error << ", retrying in 5 seconds" << std::endl;
                waitSeconds(5);
                continue;
            }

            std::cout << "Proxy listening on " << settings.mPort << ", forwarding to " << host << ":" << port
                      << std::endl;

            IncomingConnection incoming;
            while (listener.accept(incoming)) {
                std::thread(runPlayer, std::move(incoming), host, port, authentication.get(), username).detach();
                incoming = IncomingConnection();
            }

            std::cout << "The listener stopped, restarting" << std::endl;
        }
    }

}

int main() {
    for (;;) {
        try {
            run();
        } catch (const std::exception &exception) {
            std::cout << "Unexpected error: " << exception.what() << std::endl;
        } catch (...) {
            std::cout << "Unexpected error" << std::endl;
        }

        waitSeconds(1);
    }
}
