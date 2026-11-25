#include "Network/Session/SessionConnectionTarget.h"

void SessionConnectionTarget::applyTo(ClientConnectionSettings &settings) const {
    settings.mTransportLayer = mTransportLayer;

    if (mTransportLayer == TransportLayer::NetherNet) {
        settings.mNetherNet.mNetworkId = mNetworkId;
        settings.mNetherNet.mSignalingType = mSignalingType;
    } else {
        settings.mHost = mHost;
        settings.mPort = mPort;
    }

    if (!mNonce.empty())
        settings.mClientData.mNonce = mNonce;
}
