#include "Network/NetherNet/XboxRtaClient.h"

#include "Core/Debug/BedrockLog.h"

#include <chrono>
#include <cstdlib>

namespace nethernet {

    const char *RTA_CONNECT_URL = "wss://rta.xboxlive.com/connect";
    const char *RTA_SUBPROTOCOL = "rta.xboxlive.com.V2";

    namespace {

        const unsigned int TYPE_SUBSCRIBE = 1;
        const unsigned int TYPE_UNSUBSCRIBE = 2;
        const unsigned int TYPE_EVENT = 3;

        const int OPERATION_SUBSCRIBE = 0;
        const int OPERATION_UNSUBSCRIBE = 1;

        unsigned long long pendingKey(unsigned int type, unsigned int sequence) {
            return ((unsigned long long) type << 32) | sequence;
        }

        std::string quote(const std::string &value) {
            std::string out;
            out.push_back('"');

            for (char c: value) {
                if (c == '"' || c == '\\')
                    out.push_back('\\');

                out.push_back(c);
            }

            out.push_back('"');
            return out;
        }

    }

    XboxRtaClient::XboxRtaClient() {
        mSequences[OPERATION_SUBSCRIBE] = 0;
        mSequences[OPERATION_UNSUBSCRIBE] = 0;
    }

    XboxRtaClient::~XboxRtaClient() {
        close();
    }

    std::vector<std::string> XboxRtaClient::_splitTopLevel(const std::string &array) {
        std::vector<std::string> elements;

        size_t position = array.find('[');
        if (position == std::string::npos)
            return elements;

        position++;

        int depth = 0;
        bool inString = false;
        bool escaped = false;
        size_t start = std::string::npos;

        for (size_t i = position; i < array.size(); i++) {
            const char c = array[i];

            if (escaped) {
                escaped = false;
                continue;
            }

            if (inString) {
                if (c == '\\')
                    escaped = true;
                else if (c == '"')
                    inString = false;

                continue;
            }

            if (c == '"') {
                if (start == std::string::npos)
                    start = i;

                inString = true;
                continue;
            }

            if (c == '[' || c == '{') {
                if (depth == 0 && start == std::string::npos)
                    start = i;

                depth++;
                continue;
            }

            if (c == ']' || c == '}') {
                if (depth == 0) {
                    if (start != std::string::npos)
                        elements.push_back(array.substr(start, i - start));

                    break;
                }

                depth--;
                continue;
            }

            if (depth > 0)
                continue;

            if (c == ',') {
                if (start != std::string::npos)
                    elements.push_back(array.substr(start, i - start));

                start = std::string::npos;
                continue;
            }

            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                continue;

            if (start == std::string::npos)
                start = i;
        }

        for (std::string &element: elements) {
            while (!element.empty() && (element.back() == ' ' || element.back() == '\t' || element.back() == '\r' ||
                                        element.back() == '\n'))
                element.pop_back();
        }

        return elements;
    }

    std::string XboxRtaClient::_unquote(const std::string &value) {
        if (value.size() < 2 || value.front() != '"' || value.back() != '"')
            return value;

        std::string out;
        bool escaped = false;

        for (size_t i = 1; i + 1 < value.size(); i++) {
            const char c = value[i];

            if (escaped) {
                out.push_back(c);
                escaped = false;
                continue;
            }

            if (c == '\\') {
                escaped = true;
                continue;
            }

            out.push_back(c);
        }

        return out;
    }

    bool XboxRtaClient::connect(const std::string &authorization, const EventHandler &onEvent) {
        if (mSocket.isOpen())
            return false;

        mOnEvent = onEvent;

        mSocket.setSubprotocol(RTA_SUBPROTOCOL);
        mSocket.addHeader("Authorization", authorization);

        const WebSocketClient::MessageHandler handler = [this](const std::string &message) {
            _onMessage(message);
        };

        const WebSocketClient::CloseHandler closeHandler = [this](const std::string &reason) {
            LOG_WARN(LogAreaID::Network, "Xbox Live RTA connection closed: %s", reason.c_str());

            std::lock_guard<std::mutex> lock(mMutex);

            for (auto &entry: mPending) {
                entry.second->mReceived = true;
                entry.second->mStatus = RtaStatusServiceUnavailable;
            }

            mSignal.notify_all();
        };

        if (!mSocket.connect(RTA_CONNECT_URL, handler, closeHandler))
            return false;

        LOG_INFO(LogAreaID::Network, "Connected to the Xbox Live RTA service");
        return true;
    }

    void XboxRtaClient::_onMessage(const std::string &message) {
        const std::vector<std::string> elements = _splitTopLevel(message);

        if (elements.size() < 2) {
            LOG_WARN(LogAreaID::Network, "Malformed Xbox Live RTA message");
            return;
        }

        const unsigned int type = (unsigned int) strtoul(elements[0].c_str(), nullptr, 10);

        if (type == TYPE_EVENT) {
            if (elements.size() < 3)
                return;

            const unsigned int subscriptionID = (unsigned int) strtoul(elements[1].c_str(), nullptr, 10);

            if (mOnEvent != nullptr)
                mOnEvent(subscriptionID, elements[2]);

            return;
        }

        if (type != TYPE_SUBSCRIBE && type != TYPE_UNSUBSCRIBE) {
            LOG_WARN(LogAreaID::Network, "Unexpected Xbox Live RTA message type %u", type);
            return;
        }

        if (elements.size() < 3)
            return;

        const unsigned int sequence = (unsigned int) strtoul(elements[1].c_str(), nullptr, 10);

        std::lock_guard<std::mutex> lock(mMutex);

        const auto it = mPending.find(pendingKey(type, sequence));
        if (it == mPending.end())
            return;

        it->second->mReceived = true;
        it->second->mStatus = (int) strtol(elements[2].c_str(), nullptr, 10);
        it->second->mPayload.assign(elements.begin() + 3, elements.end());

        mSignal.notify_all();
    }

    bool XboxRtaClient::subscribe(const std::string &resourceUri, RtaSubscription &out, int timeoutSeconds) {
        if (!mSocket.isOpen())
            return false;

        std::shared_ptr<Pending> pending = std::make_shared<Pending>();
        unsigned int sequence;

        {
            std::lock_guard<std::mutex> lock(mMutex);
            sequence = ++mSequences[OPERATION_SUBSCRIBE];
            mPending[pendingKey(TYPE_SUBSCRIBE, sequence)] = pending;
        }

        const std::string frame = "[" + std::to_string(TYPE_SUBSCRIBE) + "," + std::to_string(sequence) + "," +
                                  quote(resourceUri) + "]";

        bool ok = false;

        if (mSocket.sendText(frame)) {
            std::unique_lock<std::mutex> lock(mMutex);
            mSignal.wait_for(lock, std::chrono::seconds(timeoutSeconds),
                             [pending]() { return pending->mReceived; });
            ok = pending->mReceived;
        }

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mPending.erase(pendingKey(TYPE_SUBSCRIBE, sequence));
        }

        if (!ok) {
            LOG_WARN(LogAreaID::Network, "Timed out subscribing to %s", resourceUri.c_str());
            return false;
        }

        if (pending->mStatus != RtaStatusOK) {
            LOG_WARN(LogAreaID::Network, "Xbox Live RTA refused the subscription to %s with code %d",
                     resourceUri.c_str(), pending->mStatus);
            return false;
        }

        if (pending->mPayload.size() < 2) {
            LOG_WARN(LogAreaID::Network, "Xbox Live RTA subscription response is missing its payload");
            return false;
        }

        out.mId = (unsigned int) strtoul(pending->mPayload[0].c_str(), nullptr, 10);
        out.mCustom = pending->mPayload[1];
        return true;
    }

    bool XboxRtaClient::unsubscribe(unsigned int subscriptionID, int timeoutSeconds) {
        if (!mSocket.isOpen())
            return false;

        std::shared_ptr<Pending> pending = std::make_shared<Pending>();
        unsigned int sequence;

        {
            std::lock_guard<std::mutex> lock(mMutex);
            sequence = ++mSequences[OPERATION_UNSUBSCRIBE];
            mPending[pendingKey(TYPE_UNSUBSCRIBE, sequence)] = pending;
        }

        const std::string frame = "[" + std::to_string(TYPE_UNSUBSCRIBE) + "," + std::to_string(sequence) + "," +
                                  std::to_string(subscriptionID) + "]";

        bool ok = false;

        if (mSocket.sendText(frame)) {
            std::unique_lock<std::mutex> lock(mMutex);
            mSignal.wait_for(lock, std::chrono::seconds(timeoutSeconds),
                             [pending]() { return pending->mReceived; });
            ok = pending->mReceived;
        }

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mPending.erase(pendingKey(TYPE_UNSUBSCRIBE, sequence));
        }

        return ok && pending->mStatus == RtaStatusOK;
    }

    void XboxRtaClient::close() {
        mSocket.close();
    }
}
