#include "Network/NetherNet/NetherNetDescription.h"

#include "Core/Json/Json.h"
#include "Network/ConnectionRequest.h"
#include "Network/NetherNet/NetherNetIdentity.h"

#include <memory>

namespace nethernet {

    namespace {

        std::vector<std::string> splitLines(const std::string &sdp) {
            std::vector<std::string> lines;
            std::string current;

            for (char c: sdp) {
                if (c == '\n') {
                    if (!current.empty() && current.back() == '\r')
                        current.pop_back();
                    lines.push_back(current);
                    current.clear();
                    continue;
                }
                current.push_back(c);
            }

            if (!current.empty())
                lines.push_back(current);

            return lines;
        }

        std::string trim(const std::string &value) {
            size_t start = 0;
            size_t end = value.size();

            while (start < end && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r'))
                start++;
            while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r'))
                end--;

            return value.substr(start, end - start);
        }

        std::vector<std::string> splitTokens(const std::string &value) {
            std::vector<std::string> tokens;
            size_t position = 0;

            while (position < value.size()) {
                while (position < value.size() && value[position] == ' ')
                    position++;

                const size_t start = position;
                while (position < value.size() && value[position] != ' ')
                    position++;

                if (position > start)
                    tokens.push_back(value.substr(start, position - start));
            }

            return tokens;
        }

        std::string toLower(const std::string &value) {
            std::string out;
            out.reserve(value.size());

            for (char c: value) {
                if (c >= 'A' && c <= 'Z')
                    out.push_back((char) (c - 'A' + 'a'));
                else
                    out.push_back(c);
            }

            return out;
        }

        std::string jsonQuote(const std::string &value) {
            std::string out;
            out.push_back('"');

            for (char c: value) {
                switch (c) {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        out.push_back(c);
                        break;
                }
            }

            out.push_back('"');
            return out;
        }

    }

    bool IdentityData::isValid() const {
        const auto validJws = [](const std::string &value) {
            if (value.empty())
                return false;

            size_t dots = 0;
            for (char c: value) {
                if (c == '.')
                    dots++;
            }

            return dots == 2;
        };

        return validJws(mAssertionToken) && validJws(mAssertionFingerprints) &&
               mIdpProtocol == "default" && !mIdpDomain.empty();
    }

    int Description::inspectOffer(const std::string &sdp) {
        // libwebrtc: ICE_UFRAG_LENGTH and ICE_PWD_LENGTH
        static const size_t USERNAME_FRAGMENT_LENGTH = 4;
        static const size_t PASSWORD_LENGTH = 24;

        bool hasUsernameFragment = false;
        bool hasPassword = false;

        for (const std::string &line: splitLines(sdp)) {
            if (line.rfind("a=ice-ufrag:", 0) == 0) {
                hasUsernameFragment = true;
                if (trim(line.substr(12)).size() != USERNAME_FRAGMENT_LENGTH)
                    return 1;
            } else if (line.rfind("a=ice-pwd:", 0) == 0) {
                hasPassword = true;
                if (trim(line.substr(10)).size() != PASSWORD_LENGTH)
                    return 1;
            } else if (line.rfind("a=candidate:", 0) == 0) {
                // libwebrtc leaves the network id and cost out of a candidate when they are zero
                std::string token;
                std::string previous;
                for (size_t index = 0; index <= line.size(); index++) {
                    if (index < line.size() && line[index] != ' ') {
                        token.push_back(line[index]);
                        continue;
                    }
                    if (token == "0" && (previous == "network-id" || previous == "network-cost"))
                        return 2;
                    previous = token;
                    token.clear();
                }
            }
        }

        return hasUsernameFragment && hasPassword ? 0 : 1;
    }

    std::vector<Fingerprint> Description::parseFingerprints(const std::string &sdp) {
        std::vector<Fingerprint> sessionLevel;
        std::vector<Fingerprint> mediaLevel;
        bool inMedia = false;

        for (const std::string &line: splitLines(sdp)) {
            if (line.rfind("m=", 0) == 0) {
                inMedia = true;
                continue;
            }

            if (line.rfind("a=fingerprint:", 0) != 0)
                continue;

            const std::string value = trim(line.substr(14));
            const size_t space = value.find(' ');
            if (space == std::string::npos)
                continue;

            Fingerprint fingerprint;
            fingerprint.mAlgorithm = value.substr(0, space);
            fingerprint.mValue = value.substr(space + 1);

            if (inMedia)
                mediaLevel.push_back(fingerprint);
            else
                sessionLevel.push_back(fingerprint);
        }

        std::vector<Fingerprint> result;

        if (!mediaLevel.empty())
            result.push_back(mediaLevel.front());
        else if (!sessionLevel.empty())
            result.push_back(sessionLevel.front());

        return result;
    }

    bool Description::parseIdentity(const std::string &sdp, IdentityData &out, std::string &failure) {
        std::string encoded;

        for (const std::string &line: splitLines(sdp)) {
            if (line.rfind("a=identity:", 0) == 0) {
                encoded = trim(line.substr(11));
                break;
            }
        }

        if (encoded.empty())
            return false;

        const std::string decoded = ConnectionRequest::decodeBase64Url(encoded);
        if (decoded.empty()) {
            failure = "failed to base64-decode a=identity attribute";
            return false;
        }

        const std::unique_ptr<json::Value> envelope = json::parse(decoded);
        if (envelope == nullptr || !envelope->isObject()) {
            failure = "a=identity payload is not a JSON object";
            return false;
        }

        const json::Value *assertion = envelope->get("assertion");
        if (assertion == nullptr || !assertion->isString()) {
            failure = "a=identity envelope is missing string 'assertion' field";
            return false;
        }

        const json::Value *idp = envelope->get("idp");
        if (idp == nullptr || !idp->isObject()) {
            failure = "a=identity envelope is missing object 'idp' field";
            return false;
        }

        const json::Value *domain = idp->get("domain");
        if (domain == nullptr || !domain->isString()) {
            failure = "a=identity envelope is missing string 'idp.domain' field";
            return false;
        }

        const json::Value *protocol = idp->get("protocol");
        if (protocol == nullptr || !protocol->isString()) {
            failure = "a=identity envelope is missing string 'idp.protocol' field";
            return false;
        }

        const std::unique_ptr<json::Value> inner = json::parse(assertion->mString);
        if (inner == nullptr || !inner->isObject()) {
            failure = "a=identity assertion is not a JSON object";
            return false;
        }

        const json::Value *fingerprints = inner->get("fingerprints");
        const json::Value *token = inner->get("token");
        if (fingerprints == nullptr || !fingerprints->isString() || token == nullptr || !token->isString()) {
            failure = "a=identity assertion is missing 'fingerprints' or 'token'";
            return false;
        }

        out.mAssertionFingerprints = fingerprints->mString;
        out.mAssertionToken = token->mString;
        out.mIdpDomain = domain->mString;
        out.mIdpProtocol = protocol->mString;
        return true;
    }

    std::string Description::generateFingerprintsPayload(const std::vector<Fingerprint> &fingerprints) {
        std::string payload = "{\"fingerprint\":[";

        for (size_t i = 0; i < fingerprints.size(); i++) {
            if (i != 0)
                payload.push_back(',');

            payload += "{\"algorithm\":";
            payload += jsonQuote(fingerprints[i].mAlgorithm);
            payload += ",\"digest\":";
            payload += jsonQuote(fingerprints[i].mValue);
            payload.push_back('}');
        }

        payload += "]}";
        return payload;
    }

    std::string Description::encodeIdentity(const IdentityData &identity) {
        std::string assertion = "{\"fingerprints\":";
        assertion += jsonQuote(identity.mAssertionFingerprints);
        assertion += ",\"token\":";
        assertion += jsonQuote(identity.mAssertionToken);
        assertion.push_back('}');

        std::string envelope = "{\"assertion\":";
        envelope += jsonQuote(assertion);
        envelope += ",\"idp\":{\"domain\":";
        envelope += jsonQuote(identity.mIdpDomain);
        envelope += ",\"protocol\":";
        envelope += jsonQuote(identity.mIdpProtocol);
        envelope += "}}";

        return Identity::encodeBase64(envelope);
    }

    std::string Description::stripCandidatePrefix(const std::string &value) {
        std::string candidate = trim(value);

        if (candidate.rfind("a=", 0) == 0)
            candidate = candidate.substr(2);

        return candidate;
    }

    std::string Description::formatCandidate(int networkId, const std::string &candidate,
                                             const std::string &usernameFragment) {
        const std::string stripped = stripCandidatePrefix(candidate);

        if (stripped.rfind("candidate:", 0) != 0)
            return stripped;

        const std::vector<std::string> tokens = splitTokens(stripped.substr(10));
        if (tokens.size() < 8)
            return stripped;

        std::string result = "candidate:";
        result += tokens[0];
        result += " 1 ";
        result += toLower(tokens[2]);

        for (size_t i = 3; i < tokens.size(); i++) {
            result.push_back(' ');
            result += tokens[i];
        }

        if (result.find(" ufrag ") != std::string::npos)
            return result;

        result += " generation 0 ufrag ";
        result += usernameFragment;
        result += " network-id ";
        result += std::to_string(networkId);
        result += " network-cost 0";
        return result;
    }

    std::string Description::buildAnswer(const AnswerParameters &parameters) {
        std::string sdp = "v=0\r\n";

        sdp += "o=- ";
        sdp += std::to_string(parameters.mSessionId);
        sdp += " 2 IN IP4 127.0.0.1\r\n";
        sdp += "s=-\r\n";
        sdp += "t=0 0\r\n";
        sdp += "a=group:BUNDLE 0\r\n";
        sdp += "a=extmap-allow-mixed\r\n";
        sdp += "a=msid-semantic: WMS\r\n";

        for (const Fingerprint &fingerprint: parameters.mFingerprints) {
            sdp += "a=fingerprint:";
            sdp += fingerprint.mAlgorithm;
            sdp.push_back(' ');
            sdp += fingerprint.mValue;
            sdp += "\r\n";
        }

        if (!parameters.mEncodedIdentity.empty()) {
            sdp += "a=identity:";
            sdp += parameters.mEncodedIdentity;
            sdp += "\r\n";
        }

        sdp += "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n";
        sdp += "c=IN IP4 0.0.0.0\r\n";

        for (size_t i = 0; i < parameters.mCandidates.size(); i++) {
            sdp += "a=";
            sdp += formatCandidate((int) i, parameters.mCandidates[i], parameters.mUsernameFragment);
            sdp += "\r\n";
        }

        sdp += "a=ice-ufrag:";
        sdp += parameters.mUsernameFragment;
        sdp += "\r\n";
        sdp += "a=ice-pwd:";
        sdp += parameters.mPassword;
        sdp += "\r\n";
        sdp += "a=ice-options:trickle\r\n";

        sdp += "a=setup:";
        sdp += parameters.mSetupRole;
        sdp += "\r\n";
        sdp += "a=mid:0\r\n";
        sdp += "a=sctp-port:5000\r\n";
        sdp += "a=max-message-size:262144\r\n";
        return sdp;
    }
}
