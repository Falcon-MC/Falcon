#pragma once

#include <array>
#include <cstdint>
#include <string>

class KeyPair;

using EncryptionKey = std::array<uint8_t, 32>;

namespace EncryptionHandshake {

    bool createServerToken(const KeyPair &serverKey, const std::string &clientPublicKeyBase64, std::string &outJwt,
                           EncryptionKey &outKey);

    bool acceptServerToken(const std::string &jwt, const KeyPair &clientKey, EncryptionKey &outKey);

}
