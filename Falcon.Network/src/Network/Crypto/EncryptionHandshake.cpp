#include "Network/Crypto/EncryptionHandshake.h"

#include "Network/ConnectionRequest.h"
#include "Network/Crypto/Base64.h"
#include "Network/Crypto/Jwt.h"
#include "Network/Crypto/KeyPair.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace {

    const size_t SALT_SIZE = 16;
    const char *SALT_KEY = "salt";

    bool deriveKey(const std::string &salt, const std::string &sharedSecret, EncryptionKey &outKey) {
        if (sharedSecret.empty())
            return false;

        const std::string material = salt + sharedSecret;
        unsigned int length = 0;
        return EVP_Digest(material.data(), material.size(), outKey.data(), &length, EVP_sha256(), nullptr) == 1
               && length == outKey.size();
    }

    std::string stripPadding(std::string value) {
        while (!value.empty() && value.back() == '=')
            value.pop_back();
        return value;
    }

}

bool EncryptionHandshake::createServerToken(const KeyPair &serverKey, const std::string &clientPublicKeyBase64,
                                            std::string &outJwt, EncryptionKey &outKey) {
    const std::shared_ptr<KeyPair> clientKey = KeyPair::fromPublicKey(clientPublicKeyBase64);
    if (clientKey == nullptr)
        return false;

    std::string salt(SALT_SIZE, '\0');
    if (RAND_bytes((unsigned char *) &salt[0], (int) salt.size()) != 1)
        return false;

    if (!deriveKey(salt, serverKey.deriveSharedSecret(*clientKey), outKey))
        return false;

    outJwt = Jwt::sign("{\"salt\":\"" + stripPadding(Base64::encode(salt)) + "\"}", serverKey);
    return !outJwt.empty();
}

bool EncryptionHandshake::acceptServerToken(const std::string &jwt, const KeyPair &clientKey, EncryptionKey &outKey) {
    Jwt::Token token;
    if (!Jwt::parse(jwt, token))
        return false;

    const std::string serverPublicKey = Jwt::readX5u(token);
    if (serverPublicKey.empty() || !Jwt::verify(token, serverPublicKey))
        return false;

    const std::shared_ptr<KeyPair> serverKey = KeyPair::fromPublicKey(serverPublicKey);
    if (serverKey == nullptr)
        return false;

    std::string salt;
    if (!Base64::decode(ConnectionRequest::findJsonString(token.mPayloadJson, SALT_KEY), salt) || salt.empty())
        return false;

    return deriveKey(salt, clientKey.deriveSharedSecret(*serverKey), outKey);
}
