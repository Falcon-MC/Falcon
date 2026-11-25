#include "Network/Crypto/Jwt.h"

#include "Network/ConnectionRequest.h"
#include "Network/Crypto/Base64.h"
#include "Network/Crypto/KeyPair.h"

namespace {

    const char *X5U_KEY = "x5u";

}

std::string Jwt::sign(const std::string &payloadJson, const KeyPair &key) {
    const std::string headerJson = "{\"alg\":\"ES384\",\"x5u\":\"" + key.getPublicKeyBase64() + "\"}";
    const std::string signingInput = Base64::encodeUrl(headerJson) + "." + Base64::encodeUrl(payloadJson);

    const std::string signature = key.sign(signingInput);
    if (signature.empty())
        return {};

    return signingInput + "." + Base64::encodeUrl(signature);
}

bool Jwt::parse(const std::string &jwt, Token &token) {
    const size_t first = jwt.find('.');
    if (first == std::string::npos)
        return false;

    const size_t second = jwt.find('.', first + 1);
    if (second == std::string::npos || jwt.find('.', second + 1) != std::string::npos)
        return false;

    token.mSigningInput = jwt.substr(0, second);
    return Base64::decodeUrl(jwt.substr(0, first), token.mHeaderJson)
           && Base64::decodeUrl(jwt.substr(first + 1, second - first - 1), token.mPayloadJson)
           && Base64::decodeUrl(jwt.substr(second + 1), token.mSignature);
}

std::string Jwt::readX5u(const Token &token) {
    return ConnectionRequest::findJsonString(token.mHeaderJson, X5U_KEY);
}

bool Jwt::verify(const Token &token, const std::string &publicKeyBase64) {
    const std::shared_ptr<KeyPair> key = KeyPair::fromPublicKey(publicKeyBase64);
    return key != nullptr && key->verify(token.mSigningInput, token.mSignature);
}
