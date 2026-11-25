#pragma once

#include "Network/Server/ServerTransport.h"

#include <memory>

namespace nethernet {
    class Connection;
}

class NetherNetServerTransport : public ServerTransport {
public:
    explicit NetherNetServerTransport(const std::shared_ptr<nethernet::Connection> &connection);

protected:
    void _closeTransport() override;

private:
    std::weak_ptr<nethernet::Connection> mConnection;
};
