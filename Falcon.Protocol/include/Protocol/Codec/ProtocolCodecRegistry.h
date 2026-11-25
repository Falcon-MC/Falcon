#pragma once

#include "Protocol/Codec/ProtocolCodec.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

/**
 * The protocol versions this build can speak, looked up from the version a client announces when it connects.
 * The default codec is the current protocol, used before a connection has chosen one.
 */
class ProtocolCodecRegistry {
public:
    static ProtocolCodecRegistry &instance();

    void registerCodec(std::shared_ptr<const ProtocolCodec> codec);

    /**
     * The codec for this protocol version, or nullptr when the version is not supported.
     */
    std::shared_ptr<const ProtocolCodec> find(int protocolVersion) const;

    const ProtocolCodec &getDefault() const { return *mDefault; }

    std::vector<int> getProtocolVersions() const;

private:
    ProtocolCodecRegistry();

    mutable std::mutex mMutex;
    std::unordered_map<int, std::shared_ptr<const ProtocolCodec>> mCodecs;
    std::shared_ptr<const ProtocolCodec> mDefault;
};
