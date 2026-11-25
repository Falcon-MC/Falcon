#pragma once

#include <string>

class KeyPair;

namespace Jwt {

    struct Token {
        std::string mHeaderJson;
        std::string mPayloadJson;
        std::string mSigningInput;
        std::string mSignature;
    };

    std::string sign(const std::string &payloadJson, const KeyPair &key);

    bool parse(const std::string &jwt, Token &token);

    std::string readX5u(const Token &token);

    bool verify(const Token &token, const std::string &publicKeyBase64);

}
