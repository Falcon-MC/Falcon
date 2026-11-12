#pragma once

#include <string>

// a uri split the way the game's script network module splits it: the authority keeps its port
struct NetUri {
    std::string mScheme;
    std::string mAuthority;
    std::string mPath;
    std::string mQuery;
    std::string mFragment;

    static NetUri split(const std::string &text);

    // the host and port of the authority; port empty when none is given
    void hostAndPort(std::string &host, std::string &port) const;

    // path and query as sent in a request line, "/" when both are empty
    std::string target() const;
};
