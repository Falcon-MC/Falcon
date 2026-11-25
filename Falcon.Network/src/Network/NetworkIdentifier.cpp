#include "Network/NetworkIdentifier.h"

#include <functional>

const NetworkIdentifier INVALID_NETWORK_IDENTIFIER;

NetworkIdentifier::NetworkIdentifier()
        : mGuid(RakNet::UNASSIGNED_RAKNET_GUID), mSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS),
          mConnectionID(0), mType(Type::Generic) {}

NetworkIdentifier::NetworkIdentifier(const RakNet::RakNetGUID &guid, const RakNet::SystemAddress &systemAddress)
        : mGuid(guid), mSystemAddress(systemAddress), mConnectionID(0), mType(Type::RakNet) {}

NetworkIdentifier::NetworkIdentifier(const std::string &networkID, unsigned long long connectionID)
        : mGuid(RakNet::UNASSIGNED_RAKNET_GUID), mSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS),
          mNetworkID(networkID), mConnectionID(connectionID), mType(Type::NetherNet) {}

std::string NetworkIdentifier::getAddress() const {
    if (mType == Type::NetherNet)
        return mRemoteAddress.empty() ? mNetworkID : mRemoteAddress;

    return mSystemAddress.ToString(false);
}

void NetworkIdentifier::setRemoteAddress(const std::string &address) {
    mRemoteAddress = address;
}

size_t NetworkIdentifier::getHash() const {
    if (mType == Type::RakNet)
        return (size_t) mGuid.g;

    if (mType == Type::NetherNet)
        return std::hash<std::string>()(mNetworkID) ^ (size_t) mConnectionID;

    return RakNet::SystemAddressHash()(mSystemAddress);
}

std::string NetworkIdentifier::toString() const {
    if (mType == Type::NetherNet) {
        const std::string name = mNetworkID + " (" + std::to_string(mConnectionID) + ")";
        return mRemoteAddress.empty() ? name : mRemoteAddress + " " + name;
    }

    return mSystemAddress.ToString() + " (" + mGuid.ToString() + ")";
}

bool NetworkIdentifier::operator==(const NetworkIdentifier &right) const {
    if (mType != right.mType)
        return false;

    if (mType == Type::RakNet)
        return mGuid == right.mGuid;

    if (mType == Type::NetherNet)
        return mConnectionID == right.mConnectionID && mNetworkID == right.mNetworkID;

    return mSystemAddress == right.mSystemAddress;
}

bool NetworkIdentifier::operator!=(const NetworkIdentifier &right) const {
    return !(*this == right);
}

size_t NetworkIdentifier::Hasher::operator()(const NetworkIdentifier &identifier) const {
    return identifier.getHash();
}
