#include "Network/Server/NetherNetServerTransport.h"

#include "Network/NetherNet/NetherNetConnection.h"

NetherNetServerTransport::NetherNetServerTransport(const std::shared_ptr<nethernet::Connection> &connection)
        : mConnection(connection) {
}

void NetherNetServerTransport::_closeTransport() {
    std::shared_ptr<nethernet::Connection> connection = mConnection.lock();
    if (connection != nullptr)
        connection->close();
}
