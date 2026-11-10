#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

// hands what the server observed of a client to the module shipped in resources/attestation.wasm and
// returns its verdict: above zero the client is to be removed, below zero it is worth a note, zero
// says nothing. What the module looks at is not part of this repository
class ClientAttestation {
public:
    enum class Evidence : uint32_t { Transport = 1 };

    static ClientAttestation &getInstance();

    ~ClientAttestation();

    // the peer is named by the certificate fingerprint of its offer
    int inspectTransport(const std::string &fingerprint);

private:
    struct Runtime;

    ClientAttestation();

    bool _load();

    std::mutex mMutex;
    bool mTried = false;
    std::unique_ptr<Runtime> mRuntime;
};
