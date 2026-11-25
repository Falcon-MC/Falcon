#include "Network/NetherNet/NetherNetConnection.h"
#include "Network/NetworkIdentifier.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void endpointsGiveTheirHost() {
        require(nethernet::Connection::hostOfEndpoint("192.0.2.14:62575") == "192.0.2.14", "IPv4 host was not read");
        require(nethernet::Connection::hostOfEndpoint("2001:db8::c718:52008") == "2001:db8::c718",
                "IPv6 host was not read");
        require(nethernet::Connection::hostOfEndpoint("::ffff:192.0.2.14:62575") == "192.0.2.14",
                "a mapped IPv4 host kept its prefix");
        require(nethernet::Connection::hostOfEndpoint("?").empty(), "an unknown endpoint gave a host");
        require(nethernet::Connection::hostOfEndpoint("").empty(), "an empty endpoint gave a host");
    }

    void theAddressIsNotTheNetworkId() {
        NetworkIdentifier id(std::string("1234567890"), 7ULL);
        require(id.getAddress() == "1234567890", "the network id is not the address before the transport is up");

        const NetworkIdentifier before = id;
        id.setRemoteAddress("192.0.2.14");
        require(id.getAddress() == "192.0.2.14", "the transport address was not used");
        require(id == before, "the address changed the identity");
        require(NetworkIdentifier::Hasher()(id) == NetworkIdentifier::Hasher()(before), "the address changed the hash");
        require(id.toString().find("192.0.2.14") != std::string::npos, "the address is missing from the text form");
    }
}

int main() {
    try {
        endpointsGiveTheirHost();
        theAddressIsNotTheNetworkId();
    } catch (const std::exception &error) {
        std::cout << "FAIL: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "PASS: peer address" << std::endl;
    return 0;
}
