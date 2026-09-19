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

        if (!line.empty())
            mNames.insert(StringUtil::toLowerCase(line));
    }
}

bool OpList::isOp(const std::string &name) const {
    return mNames.find(StringUtil::toLowerCase(name)) != mNames.end();
}

std::vector<std::string> OpList::getNames() const {
    std::vector<std::string> names;
    names.reserve(mNames.size());

    for (const std::string &name: mNames)
        names.push_back(name);

    return names;
}

void OpList::addOp(const std::string &name) {
    if (mNames.insert(StringUtil::toLowerCase(name)).second)
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

    for (const std::string &name: mNames)
        file << name << "\n";
}
