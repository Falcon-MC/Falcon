#include "Network/NetherNet/NetherNetWebSocketSignaling.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Network/Auth/AuthenticationUtils.h"
#include "Network/Auth/MinecraftAuthentication.h"
#include "Network/JsonText.h"

#include <chrono>
#include <memory>

namespace nethernet {

    namespace {

        const char *SIGNALING_SERVICE_NAME = "signaling";
        const char *SIGNALING_PATH = "/ws/v1.0/signaling/";
        const char *SERVER_SENDER = "Server";
        const char *NIL_MESSAGE_ID = "00000000-0000-0000-0000-000000000000";
        const int PING_FREQUENCY_SECONDS = 15;

        const int MESSAGE_TYPE_PING = 0;
        const int MESSAGE_TYPE_ERROR = 0;
        const int MESSAGE_TYPE_SIGNAL = 1;
        const int MESSAGE_TYPE_CREDENTIALS = 2;
        const int MESSAGE_TYPE_ACCEPTED = 3;
        const int MESSAGE_TYPE_DELIVERED = 4;

        std::string describeServiceError(const std::string &data) {
            std::unique_ptr<json::Value> root = json::parse(data);
            const json::Value *code = root != nullptr ? root->get("Code") : nullptr;
            const json::Value *message = root != nullptr ? root->get("Message") : nullptr;

            std::string description = "signaling service error code " +
                                      std::to_string(code != nullptr ? code->integer() : 0);

            if (message != nullptr && !message->string().empty())
                description += ": " + JsonText::quote(message->string());

            return description;
        }

    }

    WebSocketSignaling::WebSocketSignaling() : mHasCredentials(false), mStopping(false) {
    }

    WebSocketSignaling::~WebSocketSignaling() {
        close();
    }

    bool WebSocketSignaling::connect(MinecraftAuthentication &authentication, const std::string &networkID,
                                     std::string &outError) {
        if (mSocket.isOpen()) {
            outError = "the signaling connection is already open";
            return false;
        }

        std::string serviceUri;
        if (!authentication.requestServiceUri(SIGNALING_SERVICE_NAME, serviceUri, outError))
            return false;

        std::string authorization;
        if (!authentication.requestServiceToken(authorization, outError))
            return false;

        mNetworkID = networkID.empty() ? std::to_string(generateNetworkID()) : networkID;

        const std::string url = AuthenticationUtils::joinUrl(serviceUri, SIGNALING_PATH + mNetworkID);
        mSocket.addHeader("Authorization", authorization);

        const WebSocketClient::MessageHandler onMessage = [this](const std::string &message) {
            _onMessage(message);
        };

        const WebSocketClient::CloseHandler onClose = [this](const std::string &reason) {
            _markClosed(reason);
        };

        if (!mSocket.connect(url, onMessage, onClose)) {
            outError = "could not connect to the signaling service at " + url;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mPingMutex);
            mStopping = false;
        }

        mPingThread = std::thread(&WebSocketSignaling::_pingLoop, this);

        LOG_INFO(LogAreaID::Network, "Connected to the NetherNet signaling service as %s", mNetworkID.c_str());
        return true;
    }

    bool WebSocketSignaling::_write(int type, const std::string &to, const std::string &message,
                                    const std::string &messageID) {
        std::string frame;
        frame.reserve(message.size() + to.size() + 96);

        frame += "{\"Type\":";
        frame += std::to_string(type);

        if (!to.empty()) {
            frame += ",\"To\":";
            frame += JsonText::quote(to);
        }

        if (!message.empty()) {
            frame += ",\"Message\":";
            frame += JsonText::quote(message);
        }

        frame += ",\"MessageId\":";
        frame += JsonText::quote(messageID.empty() ? std::string(NIL_MESSAGE_ID) : messageID);
        frame += "}";

        return mSocket.sendText(frame);
    }

    bool WebSocketSignaling::signal(const Signal &signal, unsigned int timeoutMs, std::string &outError) {
        if (isClosed()) {
            outError = "signaling closed: " + getCloseReason();
            return false;
        }

        const std::string messageID = AuthenticationUtils::generateUuid();
        const std::string text = signal.toString();

        if (signal.mType != SIGNAL_TYPE_OFFER) {
            if (!_write(MESSAGE_TYPE_SIGNAL, signal.mNetworkID, text, messageID)) {
                outError = "could not write the signal to the signaling service";
                return false;
            }

            return true;
        }

        _expectDelivery(messageID);

        if (!_write(MESSAGE_TYPE_SIGNAL, signal.mNetworkID, text, messageID))
            _completeDelivery(messageID, "could not write the signal to the signaling service");

        return _awaitDelivery(messageID, timeoutMs, outError);
    }

    bool WebSocketSignaling::requestCredentials(Credentials &outCredentials, unsigned int timeoutMs,
                                                std::string &outError) {
        std::unique_lock<std::mutex> lock(mStateMutex);

        mStateSignal.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
            return mHasCredentials || mClosed;
        });

        if (mHasCredentials) {
            outCredentials = mCredentials;
            return true;
        }

        outError = mClosed ? "signaling closed before credentials were received"
                           : "timed out waiting for credentials from the signaling service";
        return false;
    }

    std::string WebSocketSignaling::getNetworkID() const {
        return mNetworkID;
    }

    void WebSocketSignaling::_onMessage(const std::string &message) {
        std::unique_ptr<json::Value> root = json::parse(message);

        if (root == nullptr || !root->isObject()) {
            LOG_WARN(LogAreaID::Network, "Malformed message from the NetherNet signaling service");
            return;
        }

        const json::Value *typeValue = root->get("Type");
        const json::Value *fromValue = root->get("From");
        const json::Value *dataValue = root->get("Message");
        const json::Value *idValue = root->get("MessageId");

        const int type = typeValue != nullptr ? typeValue->integer(-1) : -1;
        const std::string from = fromValue != nullptr ? fromValue->string() : std::string();
        const std::string data = dataValue != nullptr ? dataValue->string() : std::string();
        const std::string messageID = idValue != nullptr ? idValue->string() : std::string();

        switch (type) {
            case MESSAGE_TYPE_CREDENTIALS: {
                if (from != SERVER_SENDER) {
                    LOG_WARN(LogAreaID::Network, "Ignoring NetherNet credentials sent by %s", from.c_str());
                    return;
                }

                Credentials credentials;
                if (!credentials.parse(data)) {
                    LOG_WARN(LogAreaID::Network, "Could not decode NetherNet credentials");
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock(mStateMutex);
                    mCredentials = credentials;
                    mHasCredentials = true;
                }

                mStateSignal.notify_all();
                return;
            }

            case MESSAGE_TYPE_SIGNAL: {
                Signal signal;
                if (!signal.parse(data)) {
                    LOG_WARN(LogAreaID::Network, "Could not decode a NetherNet signal from %s", from.c_str());
                    return;
                }

                signal.mNetworkID = from;
                _dispatch(signal);
                return;
            }

            case MESSAGE_TYPE_ERROR: {
                if (messageID.empty() || messageID == NIL_MESSAGE_ID) {
                    LOG_WARN(LogAreaID::Network, "NetherNet signaling error without a message ID");
                    return;
                }

                _completeDelivery(messageID, describeServiceError(data));
                return;
            }

            case MESSAGE_TYPE_DELIVERED:
                if (!messageID.empty())
                    _completeDelivery(messageID, std::string());
                return;

            case MESSAGE_TYPE_ACCEPTED:
                return;

            default:
                LOG_WARN(LogAreaID::Network, "Unknown NetherNet signaling message type %d", type);
                return;
        }
    }

    void WebSocketSignaling::_pingLoop() {
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mPingMutex);

                if (mPingSignal.wait_for(lock, std::chrono::seconds(PING_FREQUENCY_SECONDS),
                                         [this]() {
                                             return mStopping;
                                         }))
                    return;
            }

            if (!_write(MESSAGE_TYPE_PING, std::string(), std::string(), std::string())) {
                _markClosed("could not write a ping to the signaling service");
                return;
            }
        }
    }

    void WebSocketSignaling::close() {
        {
            std::lock_guard<std::mutex> lock(mPingMutex);
            mStopping = true;
        }

        mPingSignal.notify_all();

        if (mPingThread.joinable())
            mPingThread.join();

        mSocket.close();
        _markClosed("signaling closed");
    }
}
