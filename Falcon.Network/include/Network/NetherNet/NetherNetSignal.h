#pragma once

#include <cstdint>
#include <string>

namespace nethernet {
    extern const char *SIGNAL_TYPE_OFFER;
    extern const char *SIGNAL_TYPE_ANSWER;
    extern const char *SIGNAL_TYPE_CANDIDATE;
    extern const char *SIGNAL_TYPE_ERROR;

    enum ErrorCode : int {
        ErrorCodeNone = 0,
        ErrorCodeDestinationNotLoggedIn = 1,
        ErrorCodeNegotiationTimeout = 2,
        ErrorCodeWrongTransportVersion = 3,
        ErrorCodeFailedToCreatePeerConnection = 4,
        ErrorCodeICE = 5,
        ErrorCodeConnectRequest = 6,
        ErrorCodeConnectResponse = 7,
        ErrorCodeCandidateAdd = 8,
        ErrorCodeInactivityTimeout = 9,
        ErrorCodeFailedToCreateOffer = 10,
        ErrorCodeFailedToCreateAnswer = 11,
        ErrorCodeFailedToSetLocalDescription = 12,
        ErrorCodeFailedToSetRemoteDescription = 13,
        ErrorCodeNegotiationTimeoutWaitingForResponse = 14,
        ErrorCodeNegotiationTimeoutWaitingForAccept = 15,
        ErrorCodeIncomingConnectionIgnored = 16,
        ErrorCodeSignalingParsingFailure = 17,
        ErrorCodeSignalingUnknownError = 18,
        ErrorCodeSignalingUnicastMessageDeliveryFailed = 19,
        ErrorCodeSignalingBroadcastDeliveryFailed = 20,
        ErrorCodeSignalingMessageDeliveryFailed = 21,
        ErrorCodeSignalingTurnAuthFailed = 22,
        ErrorCodeSignalingFallbackToBestEffortDelivery = 23,
        ErrorCodeNoSignalingChannel = 24,
        ErrorCodeNotLoggedIn = 25,
        ErrorCodeSignalingFailedToSend = 26,
        ErrorCodeRelayServerConfigurationResultFailure = 27,
        ErrorCodeRelayServerConfigurationResultParsingErrorNoURLs = 28,
        ErrorCodeRelayServerConfigurationResultParsingErrorNoCreds = 29,
        ErrorCodeRelayServerConfigurationResultParsingErrorNoServers = 30,
        ErrorCodeRelayServerConfigurationResultParsingErrorNoExpiration = 31,
        ErrorCodeDataChannelClosed = 32,
        ErrorCodeInternalErrorJSONSerialization = 33,
        ErrorCodeInvalidArgument = 34,
        ErrorCodeGenericFailure = 35,
        ErrorCodeIdentityVerificationFailed = 37
    };

    struct Signal {
        std::string mType;
        uint64_t mConnectionID = 0;
        std::string mData;
        std::string mNetworkID;

        bool parse(const std::string &text);

        std::string toString() const;
    };
}
