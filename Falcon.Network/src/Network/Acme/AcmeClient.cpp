#include "Network/Acme/AcmeClient.h"

#include "Core/Json/Json.h"
#include "Network/Crypto/Base64.h"
#include "Network/JsonText.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <thread>

namespace acme {

    namespace {

        const char *const JOSE_CONTENT_TYPE = "application/jose+json";
        const char *const PEM_CHAIN_TYPE = "application/pem-certificate-chain";
        const char *const BAD_NONCE = "urn:ietf:params:acme:error:badNonce";
        const char *const RATE_LIMITED = "urn:ietf:params:acme:error:rateLimited";
        const int64_t DEFAULT_POLL_SECONDS = 2;
        const int64_t MAX_POLL_SECONDS = 30;
        const size_t MAX_RESPONSE_SIZE = 1024 * 1024;
        const int64_t MAX_RETRY_AFTER_SECONDS = 24 * 3600;
        // the CA may refuse any nonce: RFC 8555 6.5 asks the client to retry with the fresh one
        const int NONCE_ATTEMPTS = 4;
        // RFC 8555 6.1: every request carries one, CAs refuse the others
        const char *const USER_AGENT = "FalconNetwork-ACME/1";

        HttpClient::Headers baseHeaders() {
            HttpClient::Headers headers;
            headers.emplace_back("User-Agent", USER_AGENT);
            return headers;
        }

        std::unique_ptr<json::Value> parseJson(const std::string &text) {
            json::Parser parser(text);
            std::unique_ptr<json::Value> value = parser.parse();
            return value != nullptr && value->isObject() ? std::move(value) : nullptr;
        }

        std::string member(const json::Value &object, const char *key) {
            const json::Value *value = object.get(key);
            return value != nullptr ? value->string() : std::string();
        }

        bool isListed(const std::string &value, const char *list) {
            std::stringstream stream(list);
            std::string candidate;
            while (std::getline(stream, candidate, ','))
                if (candidate == value)
                    return true;
            return false;
        }

        // seconds only: a date form falls back to the caller's default; capped so a bad value cannot park renewal
        int64_t retryAfter(const HttpResponse &response) {
            const std::string value = response.getHeader("retry-after");
            if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                return 0;
            if (value.size() > 9)
                return MAX_RETRY_AFTER_SECONDS;
            return std::min<int64_t>(std::strtoll(value.c_str(), nullptr, 10), MAX_RETRY_AFTER_SECONDS);
        }

        // tokens land in an URL path and an SNI lookup: base64url only
        bool isToken(const std::string &token) {
            if (token.empty() || token.size() > 256)
                return false;
            return token.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_")
                   == std::string::npos;
        }
    }

    const char *toString(ChallengeType type) {
        return type == ChallengeType::Http01 ? "http-01" : "tls-alpn-01";
    }

    bool parseChallengeType(const std::string &text, ChallengeType &outType) {
        if (text == "tls-alpn-01") {
            outType = ChallengeType::TlsAlpn01;
            return true;
        }
        if (text == "http-01") {
            outType = ChallengeType::Http01;
            return true;
        }
        return false;
    }

    Client::Client(const ClientSettings &settings, std::shared_ptr<EcKey> accountKey, ChallengeStore &challenges,
                   const std::atomic<bool> *cancel)
            : mSettings(settings), mAccountKey(std::move(accountKey)), mChallenges(challenges), mCancel(cancel) {
    }

    bool Client::_wait(int64_t seconds) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (std::chrono::steady_clock::now() < deadline) {
            if (mCancel != nullptr && mCancel->load())
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return mCancel == nullptr || !mCancel->load();
    }

    void Client::_readProblem(const HttpResponse &response, const char *step, Outcome &outcome) {
        const std::unique_ptr<json::Value> problem = parseJson(response.mBody);
        const std::string type = problem != nullptr ? member(*problem, "type") : std::string();
        const std::string detail = problem != nullptr ? member(*problem, "detail") : std::string();

        outcome.mRateLimited = type == RATE_LIMITED || response.mStatus == 429;
        outcome.mRetryAfterSeconds = retryAfter(response);
        outcome.mError = std::string(step) + " failed with HTTP " + std::to_string(response.mStatus);
        if (!detail.empty())
            outcome.mError += ": " + detail;
        else if (!type.empty())
            outcome.mError += ": " + type;
    }

    bool Client::_loadDirectory(Outcome &outcome) {
        // nonces and the account key binding travel in it: never in clear text
        if (mSettings.mDirectoryUrl.rfind("https://", 0) != 0) {
            outcome.mError = "the ACME directory must be an https:// URL, not " + mSettings.mDirectoryUrl;
            return false;
        }

        HttpResponse response;
        std::string error;
        if (!HttpClient::request("GET", mSettings.mDirectoryUrl, baseHeaders(), std::string(), response, error,
                                 mSettings.mRequestTimeoutMs, MAX_RESPONSE_SIZE, mCancel, {},
                                 mSettings.mExtraRootsFile)) {
            outcome.mError = "could not reach the ACME directory " + mSettings.mDirectoryUrl + ": " + error;
            return false;
        }
        if (response.mStatus != 200) {
            _readProblem(response, "reading the ACME directory", outcome);
            return false;
        }

        const std::unique_ptr<json::Value> directory = parseJson(response.mBody);
        if (directory != nullptr) {
            mNewNonceUrl = member(*directory, "newNonce");
            mNewAccountUrl = member(*directory, "newAccount");
            mNewOrderUrl = member(*directory, "newOrder");
        }
        if (mNewNonceUrl.empty() || mNewAccountUrl.empty() || mNewOrderUrl.empty()) {
            outcome.mError = "the ACME directory " + mSettings.mDirectoryUrl + " is not an ACME directory";
            return false;
        }
        return true;
    }

    // GET, not HEAD: the http client waits for a body a HEAD reply never sends
    bool Client::_fetchNonce(Outcome &outcome) {
        HttpResponse response;
        std::string error;
        if (!HttpClient::request("GET", mNewNonceUrl, baseHeaders(), std::string(), response, error,
                                 mSettings.mRequestTimeoutMs, MAX_RESPONSE_SIZE, mCancel, {},
                                 mSettings.mExtraRootsFile)) {
            outcome.mError = "could not fetch an ACME nonce: " + error;
            return false;
        }

        mNonce = response.getHeader("replay-nonce");
        if (mNonce.empty()) {
            outcome.mError = "the ACME server sent no nonce";
            return false;
        }
        return true;
    }

    bool Client::_post(const std::string &url, const std::string &payload, Reply &reply, Outcome &outcome,
                       const char *accept) {
        for (int attempt = 0; attempt < NONCE_ATTEMPTS; ++attempt) {
            if (mNonce.empty() && !_fetchNonce(outcome))
                return false;

            const std::string body = signJws(*mAccountKey, url, mNonce, mAccountUrl, payload);
            mNonce.clear();
            if (body.empty()) {
                outcome.mError = "could not sign the ACME request";
                return false;
            }

            HttpClient::Headers headers = baseHeaders();
            headers.emplace_back("Content-Type", JOSE_CONTENT_TYPE);
            if (accept != nullptr)
                headers.emplace_back("Accept", accept);

            reply = Reply();
            if (!HttpClient::request("POST", url, headers, body, reply.mResponse, reply.mError,
                                     mSettings.mRequestTimeoutMs, MAX_RESPONSE_SIZE, mCancel, {},
                                     mSettings.mExtraRootsFile)) {
                outcome.mError = "ACME request to " + url + " failed: " + reply.mError;
                return false;
            }

            reply.mSent = true;
            mNonce = reply.mResponse.getHeader("replay-nonce");

            if (reply.mResponse.mStatus >= 200 && reply.mResponse.mStatus < 300)
                return true;

            const std::unique_ptr<json::Value> problem = parseJson(reply.mResponse.mBody);
            if (attempt + 1 < NONCE_ATTEMPTS && problem != nullptr && member(*problem, "type") == BAD_NONCE)
                continue;

            _readProblem(reply.mResponse, "ACME request", outcome);
            return false;
        }

        return false;
    }

    bool Client::_register(Outcome &outcome) {
        std::string payload = "{\"termsOfServiceAgreed\":true";
        if (!mSettings.mEmail.empty())
            payload += ",\"contact\":[" + JsonText::quote("mailto:" + mSettings.mEmail) + "]";
        payload += "}";

        // an existing key gets its account back with 200, a new one is created with 201
        Reply reply;
        if (!_post(mNewAccountUrl, payload, reply, outcome))
            return false;

        mAccountUrl = reply.mResponse.getHeader("location");
        if (mAccountUrl.empty()) {
            outcome.mError = "the ACME server returned no account URL";
            return false;
        }
        return true;
    }

    bool Client::_pollStatus(const std::string &url, const char *pending, const char *done, std::string &outBody,
                             Outcome &outcome) {
        for (int poll = 0; poll < mSettings.mMaxPolls; ++poll) {
            Reply reply;
            if (!_post(url, std::string(), reply, outcome))
                return false;

            const std::unique_ptr<json::Value> object = parseJson(reply.mResponse.mBody);
            const std::string status = object != nullptr ? member(*object, "status") : std::string();
            if (status == done) {
                outBody = reply.mResponse.mBody;
                return true;
            }

            if (!isListed(status, pending)) {
                outBody = reply.mResponse.mBody;
                outcome.mError = "ACME object " + url + " ended as " + (status.empty() ? "unknown" : status);
                return false;
            }

            const int64_t delay = retryAfter(reply.mResponse);
            if (!_wait(std::min(delay > 0 ? delay : DEFAULT_POLL_SECONDS, MAX_POLL_SECONDS))) {
                outcome.mError = "ACME issuance cancelled";
                return false;
            }
        }

        outcome.mError = "ACME object " + url + " did not settle after " + std::to_string(mSettings.mMaxPolls)
                         + " polls";
        return false;
    }

    bool Client::_authorize(const std::string &authorizationUrl, Outcome &outcome) {
        Reply reply;
        if (!_post(authorizationUrl, std::string(), reply, outcome))
            return false;

        const std::unique_ptr<json::Value> authorization = parseJson(reply.mResponse.mBody);
        if (authorization == nullptr) {
            outcome.mError = "the ACME authorization is not JSON";
            return false;
        }

        const std::string status = member(*authorization, "status");
        if (status == "valid")
            return true;
        if (status != "pending") {
            outcome.mError = "the ACME authorization for " + mSettings.mDomain + " is " + status;
            return false;
        }

        const json::Value *challenges = authorization->get("challenges");
        std::string token;
        std::string challengeUrl;
        if (challenges != nullptr && challenges->isArray()) {
            for (const std::unique_ptr<json::Value> &challenge: challenges->mArray) {
                if (challenge->isObject() && member(*challenge, "type") == toString(mSettings.mChallenge)) {
                    token = member(*challenge, "token");
                    challengeUrl = member(*challenge, "url");
                    break;
                }
            }
        }

        if (!isToken(token) || challengeUrl.empty()) {
            outcome.mError = std::string("the ACME server offered no usable ") + toString(mSettings.mChallenge)
                             + " challenge";
            return false;
        }

        const std::string keyAuth = keyAuthorization(token, *mAccountKey);
        if (mSettings.mChallenge == ChallengeType::Http01) {
            mChallenges.putHttp(token, keyAuth);
        } else {
            std::string certificate;
            std::string key;
            if (!createTlsAlpnCertificate(mSettings.mDomain, keyAuth, certificate, key)) {
                outcome.mError = "could not build the tls-alpn-01 certificate";
                return false;
            }
            mChallenges.putTlsAlpn(mSettings.mDomain, certificate, key);
        }

        // the answer stays published until the server settles the authorization
        Reply answer;
        std::string settled;
        const bool validated = _post(challengeUrl, "{}", answer, outcome)
                               && _pollStatus(authorizationUrl, "pending,processing", "valid", settled, outcome);

        if (mSettings.mChallenge == ChallengeType::Http01)
            mChallenges.removeHttp(token);
        else
            mChallenges.removeTlsAlpn(mSettings.mDomain);

        if (!validated && !settled.empty()) {
            // the validation error sits on the challenge, not on the authorization
            const std::unique_ptr<json::Value> failed = parseJson(settled);
            const json::Value *list = failed != nullptr ? failed->get("challenges") : nullptr;
            if (list != nullptr && list->isArray()) {
                for (const std::unique_ptr<json::Value> &challenge: list->mArray) {
                    const json::Value *error = challenge->isObject() ? challenge->get("error") : nullptr;
                    if (error != nullptr && error->isObject() && !member(*error, "detail").empty())
                        outcome.mError = "validation of " + mSettings.mDomain + " failed: " + member(*error, "detail");
                }
            }
        }
        return validated;
    }

    Outcome Client::issue(const EcKey &certificateKey) {
        Outcome outcome;
        if (mAccountKey == nullptr) {
            outcome.mError = "no ACME account key";
            return outcome;
        }

        if (!_loadDirectory(outcome) || !_register(outcome))
            return outcome;

        const std::string identifiers = "{\"identifiers\":[{\"type\":\"dns\",\"value\":"
                                        + JsonText::quote(mSettings.mDomain) + "}]}";
        Reply created;
        if (!_post(mNewOrderUrl, identifiers, created, outcome))
            return outcome;

        const std::string orderUrl = created.mResponse.getHeader("location");
        const std::unique_ptr<json::Value> order = parseJson(created.mResponse.mBody);
        const std::string finalizeUrl = order != nullptr ? member(*order, "finalize") : std::string();
        const json::Value *authorizations = order != nullptr ? order->get("authorizations") : nullptr;
        if (orderUrl.empty() || finalizeUrl.empty() || authorizations == nullptr || !authorizations->isArray()) {
            outcome.mError = "the ACME order is incomplete";
            return outcome;
        }

        for (const std::unique_ptr<json::Value> &authorization: authorizations->mArray) {
            if (!authorization->isString() || !_authorize(authorization->mString, outcome))
                return outcome;
        }

        const std::string csr = createCsr(certificateKey, mSettings.mDomain);
        if (csr.empty()) {
            outcome.mError = "could not build the certificate request for " + mSettings.mDomain;
            return outcome;
        }

        Reply finalized;
        if (!_post(finalizeUrl, "{\"csr\":\"" + Base64::encodeUrl(csr) + "\"}", finalized, outcome))
            return outcome;

        std::string issued;
        if (!_pollStatus(orderUrl, "pending,ready,processing", "valid", issued, outcome))
            return outcome;

        const std::unique_ptr<json::Value> done = parseJson(issued);
        const std::string certificateUrl = done != nullptr ? member(*done, "certificate") : std::string();
        if (certificateUrl.empty()) {
            outcome.mError = "the ACME order is valid but names no certificate";
            return outcome;
        }

        Reply downloaded;
        if (!_post(certificateUrl, std::string(), downloaded, outcome, PEM_CHAIN_TYPE))
            return outcome;

        if (!certificateCovers(downloaded.mResponse.mBody, mSettings.mDomain)) {
            outcome.mError = "the issued certificate does not cover " + mSettings.mDomain;
            return outcome;
        }

        outcome.mIssued = true;
        outcome.mChainPem = downloaded.mResponse.mBody;
        return outcome;
    }

}
