#include "Protocol/Codec/ProtocolCodecRegistry.h"

#include "Protocol/Codec/Codec2169.h"
#include "Protocol/Codec/Codec2193.h"
#include "Protocol/Codec/Codec2225.h"

#include <algorithm>

ProtocolCodecRegistry &ProtocolCodecRegistry::instance() {
    static ProtocolCodecRegistry registry;
    return registry;
}

ProtocolCodecRegistry::ProtocolCodecRegistry() : mDefault(Codec2225::create()) {
    mCodecs[mDefault->getProtocolVersion()] = mDefault;

    for (const std::shared_ptr<const ProtocolCodec> &codec:
            {Codec2193::create(), Codec2169::create(), Codec2168::create()})
        mCodecs[codec->getProtocolVersion()] = codec;
}

void ProtocolCodecRegistry::registerCodec(std::shared_ptr<const ProtocolCodec> codec) {
    if (codec == nullptr)
        return;

    std::lock_guard<std::mutex> guard(mMutex);
    mCodecs[codec->getProtocolVersion()] = std::move(codec);
}

std::shared_ptr<const ProtocolCodec> ProtocolCodecRegistry::find(int protocolVersion) const {
    std::lock_guard<std::mutex> guard(mMutex);
    const auto it = mCodecs.find(protocolVersion);
    return it == mCodecs.end() ? nullptr : it->second;
}

std::vector<int> ProtocolCodecRegistry::getProtocolVersions() const {
    std::vector<int> versions;
    {
        std::lock_guard<std::mutex> guard(mMutex);
        for (const auto &entry: mCodecs)
            versions.push_back(entry.first);
    }
    std::sort(versions.begin(), versions.end());
    return versions;
}
