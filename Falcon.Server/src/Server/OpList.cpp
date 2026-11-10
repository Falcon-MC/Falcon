#include "Server/OpList.h"

#include "Core/Text/StringUtil.h"

#include <fstream>

OpList::OpList(const std::string &path) : mPath(path) {
    reload();
}

void OpList::reload() {
    mNames.clear();

    std::ifstream file(mPath);
    if (!file.is_open())
        return;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty())
            continue;

        const size_t separator = line.find('\t');
        const std::string name = StringUtil::toLowerCase(line.substr(0, separator));
        if (!name.empty())
            mNames[name] = separator == std::string::npos ? std::string() : line.substr(separator + 1);
    }
}

bool OpList::isOp(const std::string &name, const std::string &xuid) const {
    const auto found = mNames.find(StringUtil::toLowerCase(name));
    if (found == mNames.end())
        return false;

    // a freed gamertag can be taken by someone else: the recorded account keeps the rank, not the name
    return found->second.empty() || xuid.empty() || found->second == xuid;
}

void OpList::bindXuid(const std::string &name, const std::string &xuid) {
    if (xuid.empty())
        return;

    const auto found = mNames.find(StringUtil::toLowerCase(name));
    if (found == mNames.end() || !found->second.empty())
        return;

    found->second = xuid;
    _save();
}

std::vector<std::string> OpList::getNames() const {
    std::vector<std::string> names;
    names.reserve(mNames.size());

    for (const auto &entry: mNames)
        names.push_back(entry.first);

    return names;
}

void OpList::addOp(const std::string &name, const std::string &xuid) {
    const std::string lowered = StringUtil::toLowerCase(name);
    const auto found = mNames.find(lowered);
    if (found != mNames.end() && found->second == xuid)
        return;

    mNames[lowered] = xuid;
    _save();
}

void OpList::removeOp(const std::string &name) {
    if (mNames.erase(StringUtil::toLowerCase(name)) > 0)
        _save();
}

void OpList::_save() const {
    std::ofstream file(mPath, std::ios::trunc);
    if (!file.is_open())
        return;

    for (const auto &entry: mNames) {
        file << entry.first;
        if (!entry.second.empty())
            file << '\t' << entry.second;
        file << "\n";
    }
}
