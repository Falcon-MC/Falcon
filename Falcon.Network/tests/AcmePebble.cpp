#include "Network/Acme/AcmeChallengeStore.h"
#include "Network/Acme/AcmeClient.h"
#include "Network/Acme/AcmeCrypto.h"
#include "Network/Acme/AcmeManager.h"
#include "Network/NetherNet/NetherNetSignalingServer.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

// end to end against a local Pebble; tests/pebble/run.sh starts it and sets the variables, absent means skipped
namespace {
    struct PebbleSetup {
        std::string mDirectory;
        std::string mRoots;
        std::string mDomain;
        unsigned short mHttpPort = 0;
        unsigned short mTlsPort = 0;
    };

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string variable(const char *name) {
        const char *value = std::getenv(name);
        return value != nullptr ? value : std::string();
    }

    bool readSetup(PebbleSetup &setup) {
        setup.mDirectory = variable("FALCON_PEBBLE_DIRECTORY");
        setup.mRoots = variable("FALCON_PEBBLE_ROOTS");
        setup.mDomain = variable("FALCON_PEBBLE_DOMAIN");
        setup.mHttpPort = (unsigned short) std::atoi(variable("FALCON_PEBBLE_HTTP_PORT").c_str());
        setup.mTlsPort = (unsigned short) std::atoi(variable("FALCON_PEBBLE_TLS_PORT").c_str());
        return !setup.mDirectory.empty() && !setup.mRoots.empty() && !setup.mDomain.empty() && setup.mHttpPort != 0
               && setup.mTlsPort != 0;
    }

    bool answerOffer(const std::string &, const std::string &, std::string &answer, int &) {
        answer = "v=0";
        return true;
    }

    acme::ClientSettings clientSettings(const PebbleSetup &setup, acme::ChallengeType challenge) {
        acme::ClientSettings settings;
        settings.mDirectoryUrl = setup.mDirectory;
        settings.mExtraRootsFile = setup.mRoots;
        settings.mDomain = setup.mDomain;
        settings.mEmail = "operator@example.org";
        settings.mChallenge = challenge;
        return settings;
    }

    // the signaling servers stand where Pebble validates: its httpPort and tlsPort on 127.0.0.1
    struct Listeners {
        std::shared_ptr<acme::ChallengeStore> mStore = std::make_shared<acme::ChallengeStore>();
        nethernet::SignalingServer mHttp;
        nethernet::SignalingServer mTls;

        explicit Listeners(const PebbleSetup &setup) {
            mHttp.setChallengeStore(mStore);
            mTls.setChallengeStore(mStore);
            require(mHttp.start("127.0.0.1", setup.mHttpPort, answerOffer), "the http-01 listener did not start");
            require(mTls.start("127.0.0.1", setup.mTlsPort, answerOffer), "the tls-alpn-01 listener did not start");
        }

        ~Listeners() {
            mTls.stop();
            mHttp.stop();
        }
    };

    void issues(const PebbleSetup &setup, acme::ChallengeType challenge) {
        Listeners listeners(setup);
        const std::shared_ptr<acme::EcKey> accountKey = acme::EcKey::generate();
        const std::shared_ptr<acme::EcKey> certificateKey = acme::EcKey::generate();
        acme::Client client(clientSettings(setup, challenge), accountKey, *listeners.mStore, nullptr);

        const auto begin = std::chrono::steady_clock::now();
        const acme::Outcome outcome = client.issue(*certificateKey);
        const long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin).count();

        require(outcome.mIssued, std::string(acme::toString(challenge)) + " issuance failed: " + outcome.mError);
        require(acme::certificateCovers(outcome.mChainPem, setup.mDomain), "the chain misses the domain");
        require(listeners.mStore->empty(), "a challenge answer outlived its order");
        std::cout << acme::toString(challenge) << " issued in " << elapsed << " ms" << std::endl;
    }

    void reportsAFailedValidation(const PebbleSetup &setup) {
        // nothing listens for the answer: Pebble must refuse and the client must say why
        acme::ChallengeStore unpublished;
        const std::shared_ptr<acme::EcKey> accountKey = acme::EcKey::generate();
        const std::shared_ptr<acme::EcKey> certificateKey = acme::EcKey::generate();
        acme::Client client(clientSettings(setup, acme::ChallengeType::Http01), accountKey, unpublished, nullptr);

        const acme::Outcome outcome = client.issue(*certificateKey);
        require(!outcome.mIssued && !outcome.mError.empty(), "a validation nobody answered succeeded");
        std::cout << "refused as expected: " << outcome.mError << std::endl;
    }

    void managerIssuesStoresAndReloads(const PebbleSetup &setup, const std::filesystem::path &storage) {
        acme::ManagerSettings settings;
        settings.mClient = clientSettings(setup, acme::ChallengeType::TlsAlpn01);
        settings.mStorageDirectory = storage.string();

        {
            Listeners listeners(setup);
            require(!listeners.mTls.isSecure(), "secure before issuance");
            acme::Manager manager(settings, listeners.mStore, [&listeners](const std::string &certificate,
                                                                           const std::string &key) {
                return listeners.mTls.reloadCertificate(certificate, key);
            });
            require(manager.start(), "the manager did not start");

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
            while (!listeners.mTls.isSecure() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            require(listeners.mTls.isSecure(), "the manager never installed a certificate");
            manager.stop();

#ifndef _WIN32
            const auto permissions = std::filesystem::status(manager.getKeyPath()).permissions();
            require((permissions & (std::filesystem::perms::group_all | std::filesystem::perms::others_all))
                    == std::filesystem::perms::none, "the certificate key is readable by others");
            const auto account = std::filesystem::status(
                    std::filesystem::path(manager.getKeyPath()).parent_path() / "account.key").permissions();
            require((account & (std::filesystem::perms::group_all | std::filesystem::perms::others_all))
                    == std::filesystem::perms::none, "the account key is readable by others");
#endif
        }

        // a restart serves the stored certificate before any request leaves
        Listeners restarted(setup);
        acme::Manager again(settings, restarted.mStore, [&restarted](const std::string &certificate,
                                                                     const std::string &key) {
            return restarted.mTls.reloadCertificate(certificate, key);
        });
        require(again.start(), "the restarted manager did not start");
        require(restarted.mTls.isSecure(), "the stored certificate was not loaded at start");
        again.stop();
    }
}

int main() {
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif

    PebbleSetup setup;
    if (!readSetup(setup)) {
        std::cout << "SKIP: acme pebble (FALCON_PEBBLE_* not set)" << std::endl;
        return 0;
    }

    const std::filesystem::path storage = std::filesystem::temp_directory_path() / "falcon-acme-pebble";
    std::filesystem::remove_all(storage);

    try {
        issues(setup, acme::ChallengeType::TlsAlpn01);
        issues(setup, acme::ChallengeType::Http01);
        reportsAFailedValidation(setup);
        managerIssuesStoresAndReloads(setup, storage);
    } catch (const std::exception &error) {
        std::cout << "FAIL: " << error.what() << std::endl;
        std::filesystem::remove_all(storage);
        return 1;
    }

    std::filesystem::remove_all(storage);
    std::cout << "PASS: acme pebble" << std::endl;
    return 0;
}
