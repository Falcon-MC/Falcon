#include "Network/Client/ClientConnectionRequest.h"
#include "Network/Crypto/Jwt.h"
#include "Network/Crypto/KeyPair.h"
#include "Core/Json/Json.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main() {
    try {
        const auto clientKey = KeyPair::generate();
        const auto serverKey = KeyPair::generate();
        require(clientKey && serverKey, "generate fixture keys");
        const auto chain = "{\"chain\":[\"" + Jwt::sign("{}", *serverKey) + "\"]}";
        for (const auto &version : {"1.21.90", "1.26.29", "1.26.30", "1.26.51", "1.26.52"}) {
            ClientData data;
            data.mGameVersion = version;
            for (bool legacy : {false, true}) {
                std::string auth, client, error;
                require(ClientConnectionRequest::createOnline(chain, "fixture-token", data, *clientKey,
                    legacy, auth, client, error), "create online login request");
                const auto envelope = json::parse(auth);
                require(envelope && envelope->isObject(), "decode login envelope");
                if (legacy) {
                    require(envelope->get("chain") != nullptr, "legacy certificate missing");
                } else {
                    const bool transitional = data.mGameVersion == "1.21.90" || data.mGameVersion == "1.26.29";
                    require((envelope->get("Certificate") != nullptr) == transitional,
                        "certificate presence differs from the game version");
                    const auto token = envelope->get("Token");
                    require(token && token->string() == "fixture-token", "multiplayer token changed");
                    require(envelope->get("AuthenticationType")->integer() == 0, "authentication type changed");
                }
                Jwt::Token signedClient;
                require(Jwt::parse(client, signedClient) && Jwt::verify(signedClient, clientKey->getPublicKeyBase64()),
                    "client data signature invalid");
            }
        }
        ClientData offlineData;
        offlineData.mGameVersion = "1.26.52";
        std::string auth, client, error;
        require(ClientConnectionRequest::createOffline({}, offlineData, *clientKey, false, auth, client, error),
            "create offline login request");
        const auto offline = json::parse(auth);
        require(offline && offline->get("Certificate") && offline->get("AuthenticationType")->integer() == 2,
            "offline certificate changed");
        std::cout << "Client login envelope compatibility tests passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
