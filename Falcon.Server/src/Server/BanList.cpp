#include "Server/BanList.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Core/Text/StringUtil.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <memory>
#include <sstream>

namespace {
    const char *DEFAULT_REASON = "Banned by an operator.";
    const char *FOREVER = "Forever";
}

BanList::BanList(const std::string &path) : mPath(path) {
    reload();
}

std::string BanList::_currentDate() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %z", &local);
    return buffer;
}

void BanList::reload() {
    mEntries.clear();

    std::ifstream file(mPath);
    if (!file.is_open()) {
        _save();
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string source = buffer.str();

    const std::unique_ptr<json::Value> root = json::parse(source);
    if (root == nullptr || root->mType != json::Value::Type::Array) {
        LOG_WARN(LogAreaID::Server, "Failed to parse %s", mPath.c_str());
        return;
    }

    for (const std::unique_ptr<json::Value> &value: root->mArray) {
        if (value->mType != json::Value::Type::Object)
            continue;

        const json::Value *name = value->get("name");
        if (name == nullptr || name->string().empty())
            continue;

        BanEntry entry;
        entry.mName = StringUtil::toLowerCase(name->string());

        const json::Value *creationDate = value->get("creationDate");
        const json::Value *banSource = value->get("source");
        const json::Value *expireDate = value->get("expireDate");
        const json::Value *reason = value->get("reason");

        entry.mCreationDate = creationDate != nullptr ? creationDate->string() : _currentDate();
        entry.mSource = banSource != nullptr ? banSource->string("(Unknown)") : "(Unknown)";
        entry.mExpireDate = expireDate != nullptr ? expireDate->string(FOREVER) : FOREVER;
        entry.mReason = reason != nullptr ? reason->string(DEFAULT_REASON) : DEFAULT_REASON;
        mEntries.push_back(entry);
    }
}

const BanEntry *BanList::find(const std::string &name) const {
    const std::string lowered = StringUtil::toLowerCase(name);
    const auto found = std::find_if(mEntries.begin(), mEntries.end(), [&lowered](const BanEntry &entry) {
        return entry.mName == lowered;
    });
    return found == mEntries.end() ? nullptr : &*found;
}

void BanList::add(const std::string &name, const std::string &reason, const std::string &source) {
    remove(name);

    BanEntry entry;
    entry.mName = StringUtil::toLowerCase(name);
    entry.mCreationDate = _currentDate();
    entry.mSource = source;
    entry.mExpireDate = FOREVER;
    entry.mReason = reason.empty() ? DEFAULT_REASON : reason;
    mEntries.push_back(entry);
    _save();
}

bool BanList::remove(const std::string &name) {
    const std::string lowered = StringUtil::toLowerCase(name);
    const auto found = std::find_if(mEntries.begin(), mEntries.end(), [&lowered](const BanEntry &entry) {
        return entry.mName == lowered;
    });
    if (found == mEntries.end())
        return false;

    mEntries.erase(found);
    _save();
    return true;
}

std::vector<std::string> BanList::getNames() const {
    std::vector<std::string> names;
    names.reserve(mEntries.size());

    for (const BanEntry &entry: mEntries)
        names.push_back(entry.mName);

    return names;
}

void BanList::_save() const {
    std::ofstream file(mPath, std::ios::trunc);
    if (!file.is_open())
        return;

    if (mEntries.empty()) {
        file << "[]\n";
        return;
    }

    file << "[\n";
    for (size_t index = 0; index < mEntries.size(); ++index) {
        const BanEntry &entry = mEntries[index];
        file << "    {\n";
        file << "        \"name\": \"" << json::escape(entry.mName) << "\",\n";
        file << "        \"creationDate\": \"" << json::escape(entry.mCreationDate) << "\",\n";
        file << "        \"source\": \"" << json::escape(entry.mSource) << "\",\n";
        file << "        \"expireDate\": \"" << json::escape(entry.mExpireDate) << "\",\n";
        file << "        \"reason\": \"" << json::escape(entry.mReason) << "\"\n";
        file << "    }" << (index + 1 < mEntries.size() ? "," : "") << "\n";
    }
    file << "]\n";
}
