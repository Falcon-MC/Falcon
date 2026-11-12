#include "Core/Net/NetUri.h"

NetUri NetUri::split(const std::string &text) {
    NetUri uri;
    size_t pathStart = 0;
    // sub_1403F0480 (26.60): "://" ends the scheme, then the authority runs to the first '/' or '?'
    const size_t schemeEnd = text.size() >= 3 ? text.find("://") : std::string::npos;
    if (schemeEnd != std::string::npos) {
        uri.mScheme = text.substr(0, schemeEnd);
        const size_t authorityStart = schemeEnd + 3;
        const size_t authorityEnd = text.find_first_of("/?", authorityStart);
        uri.mAuthority = text.substr(authorityStart, authorityEnd == std::string::npos ? std::string::npos
                                                                                       : authorityEnd - authorityStart);
        pathStart = authorityEnd == std::string::npos ? text.size() : authorityEnd;
    }

    size_t end = text.size();
    const size_t hash = text.find('#');
    if (hash != std::string::npos) {
        uri.mFragment = text.substr(hash + 1);
        end = hash;
    }
    const size_t question = text.find('?');
    size_t pathEnd = end;
    if (question != std::string::npos && question < end) {
        uri.mQuery = text.substr(question + 1, end - question - 1);
        pathEnd = question;
    }
    if (pathEnd > pathStart)
        uri.mPath = text.substr(pathStart, pathEnd - pathStart);
    return uri;
}

void NetUri::hostAndPort(std::string &host, std::string &port) const {
    std::string authority = mAuthority;
    const size_t at = authority.rfind('@');
    if (at != std::string::npos)
        authority = authority.substr(at + 1);
    port.clear();
    if (!authority.empty() && authority[0] == '[') {
        const size_t close = authority.find(']');
        host = authority.substr(1, close == std::string::npos ? std::string::npos : close - 1);
        if (close != std::string::npos && close + 1 < authority.size() && authority[close + 1] == ':')
            port = authority.substr(close + 2);
        return;
    }
    const size_t colon = authority.rfind(':');
    host = colon == std::string::npos ? authority : authority.substr(0, colon);
    if (colon != std::string::npos)
        port = authority.substr(colon + 1);
}

std::string NetUri::target() const {
    std::string target = mPath.empty() ? "/" : mPath;
    if (!mQuery.empty())
        target += "?" + mQuery;
    return target;
}
