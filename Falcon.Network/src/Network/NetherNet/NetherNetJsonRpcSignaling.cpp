#include "Network/NetherNet/NetherNetJsonRpcSignaling.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Network/Auth/AuthenticationUtils.h"
#include "Network/Auth/MinecraftAuthentication.h"
#include "Network/Crypto/Jwt.h"
#include "Network/Http/HttpClient.h"
#include "Network/JsonText.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <vector>

namespace nethernet {

    namespace {

        const char *AFD_SERVICE_NAME = "signaling-afd";
        const char *CONFIGURATION_PATH = "/api/v1.0/configuration";
        const char *MESSAGING_PATH = "/ws/v1.0/messaging/connect";
        const char *SERVICE_USER_AGENT = "libhttpclient/1.0.0.0";
        const char *SERVICE_TOKEN_PREFIX = "MCToken ";

        const char *METHOD_SYSTEM_PING = "System_Ping_v1_0";
        const char *METHOD_SYSTEM_PONG = "System_Pong_v1_0";
        const char *METHOD_CREDENTIALS = "Signaling_TurnAuth_v1_0";
        // stale this long before they expire: the NetherNet instance asks again at that point
        const int64_t CREDENTIALS_REFRESH_MARGIN_SECONDS = 60;
        const char *METHOD_RECEIVE_MESSAGE = "Signaling_ReceiveMessage_v1_0";
        const char *METHOD_SEND_MESSAGE = "Signaling_SendClientMessage_v1_0";
        const char *METHOD_DELIVERY_NOTIFICATION = "Signaling_DeliveryNotification_V1_0";
        const char *METHOD_WEBRTC = "Signaling_WebRtc_v1_0";

        const int DEFAULT_PING_FREQUENCY_SECONDS = 15;
        const unsigned int PING_TIMEOUT_MS = 5000;
        const unsigned int ACKNOWLEDGE_TIMEOUT_MS = 15000;

        bool parsePingFrequency(const std::string &value, int &outSeconds) {
            int parts[3] = {0, 0, 0};
            size_t part = 0;
            size_t start = 0;

            for (size_t index = 0; index <= value.size(); ++index) {
                if (index < value.size() && value[index] != ':')
                    continue;

                if (part >= 3 || index == start)
                    return false;

                for (size_t digit = start; digit < index; ++digit) {
                    if (!std::isdigit((unsigned char) value[digit]))
                        return false;
                }

                parts[part++] = atoi(value.substr(start, index - start).c_str());
                start = index + 1;
            }

            if (part != 3)
                return false;

            const int seconds = parts[0] * 3600 + parts[1] * 60 + parts[2];
            if (seconds <= 0)
                return false;

            outSeconds = seconds;
            return true;
        }

        std::string buildRequest(unsigned long long id, const std::string &method, const std::string &params) {
            std::string request;
            request.reserve(params.size() + method.size() + 64);
            request += "{\"jsonrpc\":\"2.0\",\"id\":";
            request += std::to_string(id);
            request += ",\"method\":";
            request += JsonText::quote(method);
            request += ",\"params\":";
            request += params;
            request += "}";
            return request;
        }

    }

    JsonRpcSignaling::JsonRpcSignaling()
            : mNextCallID(1), mCredentialsExpiry(0), mStopping(false),
              mPingFrequencySeconds(DEFAULT_PING_FREQUENCY_SECONDS) {
    }

    JsonRpcSignaling::~JsonRpcSignaling() {
        close();
    }

    bool JsonRpcSignaling::isMessagingID(const std::string &value) {
        if (value.size() != 36)
            return false;

        for (size_t index = 0; index < value.size(); ++index) {
            const char character = value[index];

            if (index == 8 || index == 13 || index == 18 || index == 23) {
                if (character != '-')
                    return false;

                continue;
            }

            if (!std::isxdigit((unsigned char) character))
                return false;
        }

        return true;
    }

    std::string JsonRpcSignaling::normalizeMessagingID(const std::string &value) {
        std::string normalized;
        normalized.reserve(value.size());

        for (char character: value)
            normalized.push_back((char) std::tolower((unsigned char) character));

        return normalized;
    }

    const std::string &JsonRpcSignaling::getPlayerMessagingID() const {
        return mPlayerMessagingID;
    }

    bool JsonRpcSignaling::connect(MinecraftAuthentication &authentication, const std::string &networkID,
                                   std::string &outError) {
        if (mSocket.isOpen()) {
            outError = "the messaging connection is already open";
            return false;
        }

        std::string afdUri;
        if (!authentication.requestServiceUri(AFD_SERVICE_NAME, afdUri, outError))
            return false;

        std::string authorization;
        if (!authentication.requestServiceToken(authorization, outError))
            return false;

        std::string token = authorization;
        if (token.rfind(SERVICE_TOKEN_PREFIX, 0) == 0)
            token = token.substr(std::string(SERVICE_TOKEN_PREFIX).size());

        Jwt::Token parsed;
        if (!Jwt::parse(token, parsed)) {
            outError = "could not decode the service token";
            return false;
        }

        std::unique_ptr<json::Value> claims = json::parse(parsed.mPayloadJson);
        const json::Value *pmid = claims != nullptr ? claims->get("pmid") : nullptr;

        if (pmid == nullptr || !isMessagingID(pmid->string())) {
            outError = "the service token does not claim a player messaging ID";
            return false;
        }

        mPlayerMessagingID = normalizeMessagingID(pmid->string());

        HttpClient::Headers headers;
        headers.emplace_back("User-Agent", SERVICE_USER_AGENT);
        headers.emplace_back("Authorization", authorization);

        const std::string configurationUrl = AuthenticationUtils::joinUrl(afdUri, CONFIGURATION_PATH);

        HttpResponse response;
        if (!HttpClient::get(configurationUrl, headers, response, outError))
            return false;

        if (response.mStatus != 200) {
            outError = "GET " + configurationUrl + ": status " + std::to_string(response.mStatus);
            return false;
        }

        std::unique_ptr<json::Value> root = json::parse(response.mBody);
        const json::Value *result = root != nullptr ? root->get("result") : nullptr;
        const json::Value *signalingUri = result != nullptr ? result->get("signalingUri") : nullptr;
        const json::Value *pingFrequency = result != nullptr ? result->get("pingFrequency") : nullptr;

        if (signalingUri == nullptr || signalingUri->string().empty()) {
            outError = "the signaling configuration has no signalingUri";
            return false;
        }

        mPingFrequencySeconds = DEFAULT_PING_FREQUENCY_SECONDS;

        if (pingFrequency != nullptr && !pingFrequency->string().empty() &&
            !parsePingFrequency(pingFrequency->string(), mPingFrequencySeconds)) {
            outError = "invalid signaling ping frequency " + pingFrequency->string();
            return false;
        }

        mNetworkID = networkID.empty() ? std::to_string(generateNetworkID()) : networkID;

        const std::string url = AuthenticationUtils::joinUrl(signalingUri->string(), MESSAGING_PATH);
        mSocket.addHeader("Authorization", authorization);

        const WebSocketClient::MessageHandler onMessage = [this](const std::string &message) {
            _onMessage(message);
        };

        const WebSocketClient::CloseHandler onClose = [this](const std::string &reason) {
            _markClosed(reason);
        };

        if (!mSocket.connect(url, onMessage, onClose)) {
            outError = "could not connect to the messaging service at " + url;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mPingMutex);
            mStopping = false;
        }

        mPingThread = std::thread(&JsonRpcSignaling::_pingLoop, this);

        LOG_INFO(LogAreaID::Network, "Connected to the NetherNet messaging service as %s",
                 mPlayerMessagingID.c_str());
        return true;
    }

    bool JsonRpcSignaling::_call(const std::string &method, const std::string &params, unsigned int timeoutMs,
                                 std::string *outResult, std::string &outError) {
        unsigned long long id;

        {
            std::lock_guard<std::mutex> lock(mStateMutex);

            if (mClosed) {
                outError = "messaging connection closed";
                return false;
            }

            id = mNextCallID++;
            mCalls[id] = PendingCall();
        }

        if (!mSocket.sendText(buildRequest(id, method, params))) {
            std::lock_guard<std::mutex> lock(mStateMutex);
            mCalls.erase(id);
            outError = "could not write the " + method + " call";
            return false;
        }

        std::unique_lock<std::mutex> lock(mStateMutex);

        const auto it = mCalls.find(id);

        mStateSignal.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, &it]() {
            return it->second.mDone || mClosed;
        });

        const PendingCall call = it->second;
        mCalls.erase(it);

        if (!call.mDone) {
            outError = mClosed ? "messaging connection closed during " + method : "call " + method + " timed out";
            return false;
        }

        if (!call.mError.empty()) {
            outError = "call " + method + ": " + call.mError;
            return false;
        }

        if (outResult != nullptr)
            *outResult = call.mResult;

        return true;
    }

    bool JsonRpcSignaling::_post(const std::string &method, const std::string &params) {
        unsigned long long id;

        {
            std::lock_guard<std::mutex> lock(mStateMutex);

            if (mClosed)
                return false;

            id = mNextCallID++;
        }

        return mSocket.sendText(buildRequest(id, method, params));
    }

    bool JsonRpcSignaling::_sendEnvelope(const std::string &messageID, const std::string &inner,
                                         const std::string &recipient, bool awaitResponse, unsigned int timeoutMs,
                                         std::string &outError) {
        std::string params;
        params.reserve(inner.size() * 2 + 128);
        params += "{\"message\":";
        params += JsonText::quote(inner);
        params += ",\"messageId\":";
        params += JsonText::quote(messageID);
        params += ",\"toPlayerId\":";
        params += JsonText::quote(recipient);
        params += "}";

        if (!awaitResponse) {
            if (!_post(METHOD_SEND_MESSAGE, params)) {
                outError = "could not write the message to the messaging service";
                return false;
            }

            return true;
        }

        return _call(METHOD_SEND_MESSAGE, params, timeoutMs, nullptr, outError);
    }

    bool JsonRpcSignaling::signal(const Signal &signal, unsigned int timeoutMs, std::string &outError) {
        if (!isMessagingID(signal.mNetworkID)) {
            outError = "recipient ID must be a player messaging UUID: " + signal.mNetworkID;
            return false;
        }

        const std::string recipient = normalizeMessagingID(signal.mNetworkID);
        const std::string messageID = AuthenticationUtils::generateUuid();

        std::string inner = "{\"jsonrpc\":\"2.0\",\"method\":";
        inner += JsonText::quote(METHOD_WEBRTC);
        inner += ",\"params\":{\"message\":";
        inner += JsonText::quote(signal.toString());
        inner += ",\"netherNetId\":";
        inner += JsonText::quote(mNetworkID);
        inner += "}}";

        if (signal.mType != SIGNAL_TYPE_OFFER)
            return _sendEnvelope(messageID, inner, recipient, true, timeoutMs, outError);

        _expectDelivery(messageID);

        std::string sendError;
        if (!_sendEnvelope(messageID, inner, recipient, true, timeoutMs, sendError))
            _completeDelivery(messageID, sendError);

        return _awaitDelivery(messageID, timeoutMs, outError);
    }

    bool JsonRpcSignaling::requestCredentials(Credentials &outCredentials, unsigned int timeoutMs,
                                              std::string &outError) {
        std::lock_guard<std::mutex> lock(mCredentialsMutex);

        const int64_t now = AuthenticationUtils::currentUnixTime();

        if (!mCredentials.mIceServers.empty() && now + CREDENTIALS_REFRESH_MARGIN_SECONDS < mCredentialsExpiry) {
            outCredentials = mCredentials;
            return true;
        }

        std::string result;
        if (!_call(METHOD_CREDENTIALS, "{}", timeoutMs, &result, outError))
            return false;

        Credentials credentials;
        if (!credentials.parse(result) || credentials.mExpirationInSeconds == 0) {
            outError = "call " + std::string(METHOD_CREDENTIALS) + ": invalid credentials";
            return false;
        }

        mCredentials = credentials;
        mCredentialsExpiry = now + credentials.mExpirationInSeconds;
        outCredentials = mCredentials;
        return true;
    }

    std::string JsonRpcSignaling::getNetworkID() const {
        return mNetworkID;
    }

    void JsonRpcSignaling::_onMessage(const std::string &message) {
        std::unique_ptr<json::Value> root = json::parse(message);

        if (root == nullptr || !root->isObject()) {
            LOG_WARN(LogAreaID::Network, "Malformed message from the NetherNet messaging service");
            return;
        }

        const json::Value *method = root->get("method");

        if (method != nullptr && method->isString()) {
            std::string rawID;
            const bool hasID = JsonText::findMember(message, "id", false, rawID) && rawID != "null";

            if (method->mString == METHOD_RECEIVE_MESSAGE) {
                std::string rawParams;
                std::vector<std::string> envelopes;

                const bool found = JsonText::findMember(message, "params", false, rawParams);
                const size_t first = found ? rawParams.find_first_not_of(" \t\r\n") : std::string::npos;

                if (first != std::string::npos && rawParams[first] == '{') {
                    _handleEnvelope(rawParams);
                } else if (found && JsonText::splitArray(rawParams, envelopes)) {
                    for (const std::string &envelope: envelopes)
                        _handleEnvelope(envelope);
                } else {
                    LOG_WARN(LogAreaID::Network, "Could not decode the envelopes of %s", METHOD_RECEIVE_MESSAGE);
                }
            } else if (method->mString != METHOD_SYSTEM_PONG) {
                LOG_WARN(LogAreaID::Network, "Unknown NetherNet messaging method %s", method->mString.c_str());
            }

            if (hasID)
                mSocket.sendText("{\"jsonrpc\":\"2.0\",\"id\":" + rawID + ",\"result\":null}");

            return;
        }

        const json::Value *idValue = root->get("id");
        if (idValue == nullptr || !idValue->isNumber())
            return;

        const unsigned long long id = (unsigned long long) idValue->number();
        const json::Value *error = root->get("error");

        std::string result;
        std::string failure;

        if (error != nullptr && error->isObject()) {
            const json::Value *code = error->get("code");
            const json::Value *text = error->get("message");
            failure = "code " + std::to_string(code != nullptr ? code->integer() : 0) + ": " +
                      (text != nullptr ? text->string() : std::string());
        } else if (!JsonText::findMember(message, "result", false, result)) {
            result = "null";
        }

        {
            std::lock_guard<std::mutex> lock(mStateMutex);

            const auto it = mCalls.find(id);
            if (it == mCalls.end())
                return;

            it->second.mDone = true;
            it->second.mResult = std::move(result);
            it->second.mError = std::move(failure);
        }

        mStateSignal.notify_all();
    }

    void JsonRpcSignaling::_handleEnvelope(const std::string &envelope) {
        std::unique_ptr<json::Value> root = json::parse(envelope);

        if (root == nullptr || !root->isObject()) {
            LOG_WARN(LogAreaID::Network, "Malformed NetherNet messaging envelope");
            return;
        }

        const json::Value *fromValue = JsonText::getMember(*root, "From", true);
        const json::Value *idValue = JsonText::getMember(*root, "Id", true);
        const json::Value *messageValue = JsonText::getMember(*root, "Message", true);

        if (fromValue == nullptr || messageValue == nullptr || !messageValue->isString()) {
            LOG_WARN(LogAreaID::Network, "NetherNet messaging envelope is missing From or Message");
            return;
        }

        const std::string from = normalizeMessagingID(fromValue->string());
        const std::string envelopeID = idValue != nullptr ? normalizeMessagingID(idValue->string()) : std::string();

        std::unique_ptr<json::Value> inner = json::parse(messageValue->mString);

        if (inner == nullptr || !inner->isObject()) {
            LOG_WARN(LogAreaID::Network, "NetherNet messaging envelope carries a malformed message");
            return;
        }

        const json::Value *method = inner->get("method");
        const json::Value *params = inner->get("params");

        if (method != nullptr && method->string() == METHOD_DELIVERY_NOTIFICATION) {
            const json::Value *messageID = params != nullptr ? params->get("messageId") : nullptr;

            if (messageID != nullptr && !messageID->string().empty())
                _completeDelivery(normalizeMessagingID(messageID->string()), std::string());

            return;
        }

        if (method != nullptr && method->string() == METHOD_WEBRTC) {
            const json::Value *data = params != nullptr ? params->get("message") : nullptr;

            Signal signal;
            if (data == nullptr || !signal.parse(data->string())) {
                LOG_WARN(LogAreaID::Network, "Could not decode a NetherNet signal from %s", from.c_str());
                return;
            }

            signal.mNetworkID = from;
            _dispatch(signal);

            std::string acknowledgement = "{\"jsonrpc\":\"2.0\",\"method\":";
            acknowledgement += JsonText::quote(METHOD_DELIVERY_NOTIFICATION);
            acknowledgement += ",\"params\":{\"messageId\":";
            acknowledgement += JsonText::quote(envelopeID);
            acknowledgement += "}}";

            std::string error;
            if (!_sendEnvelope(AuthenticationUtils::generateUuid(), acknowledgement, from, false,
                               ACKNOWLEDGE_TIMEOUT_MS, error)) {
                LOG_WARN(LogAreaID::Network, "Could not acknowledge a NetherNet signal: %s", error.c_str());
            }

            return;
        }

        const json::Value *code = inner->get("Code");

        if (method == nullptr && code != nullptr && code->isNumber()) {
            const json::Value *text = inner->get("Message");
            std::string description = "signaling service error code " + std::to_string(code->integer());

            if (text != nullptr && !text->string().empty())
                description += ": " + JsonText::quote(text->string());

            _completeDelivery(envelopeID, description);
            return;
        }

        LOG_WARN(LogAreaID::Network, "Unknown NetherNet messaging inner method %s",
                 method != nullptr ? method->string().c_str() : "(none)");
    }

    void JsonRpcSignaling::_pingLoop() {
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mPingMutex);

                if (mPingSignal.wait_for(lock, std::chrono::seconds(mPingFrequencySeconds),
                                         [this]() {
                                             return mStopping;
                                         }))
                    return;
            }

            std::string error;
            if (!_call(METHOD_SYSTEM_PING, "{}", PING_TIMEOUT_MS, nullptr, error)) {
                LOG_WARN(LogAreaID::Network, "NetherNet messaging ping failed: %s", error.c_str());
                _markClosed("call " + std::string(METHOD_SYSTEM_PING) + ": " + error);
                return;
            }
        }
    }

    void JsonRpcSignaling::close() {
        {
            std::lock_guard<std::mutex> lock(mPingMutex);
            mStopping = true;
        }

        mPingSignal.notify_all();

        if (mPingThread.joinable())
            mPingThread.join();

        mSocket.close();
        _markClosed("messaging closed");
    }
}
