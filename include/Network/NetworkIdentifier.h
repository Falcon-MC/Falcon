#pragma once

#include "RakNet/RakNetTypes.h"

#include <cstddef>
#include <string>

class NetworkIdentifier {
public:
    enum class Type : unsigned char {
        RakNet = 0,
        NetherNet = 1,
        Generic = 2
    };

    struct Hasher {
        size_t operator()(const NetworkIdentifier &identifier) const;
    };

    NetworkIdentifier();

    NetworkIdentifier(const RakNet::RakNetGUID &guid, const RakNet::SystemAddress &systemAddress);

    NetworkIdentifier(const std::string &networkID, unsigned long long connectionID);

    Type getType() const { return mType; }

    const std::string &getNetworkID() const { return mNetworkID; }

    unsigned long long getConnectionID() const { return mConnectionID; }

    const RakNet::RakNetGUID &getGuid() const { return mGuid; }

    const RakNet::SystemAddress &getSystemAddress() const { return mSystemAddress; }

    // a NetherNet peer names its own network id: once its transport is up this is the host its packets
    // come from instead
    std::string getAddress() const;

    // never part of the identity: two identifiers of one connection stay equal before and after
    void setRemoteAddress(const std::string &address);

    size_t getHash() const;

    std::string toString() const;

    bool operator==(const NetworkIdentifier &right) const;

    bool operator!=(const NetworkIdentifier &right) const;

private:
    RakNet::RakNetGUID mGuid;
    RakNet::SystemAddress mSystemAddress;
    std::string mNetworkID;
    std::string mRemoteAddress;
    unsigned long long mConnectionID;
    Type mType;
};

extern const NetworkIdentifier INVALID_NETWORK_IDENTIFIER;
