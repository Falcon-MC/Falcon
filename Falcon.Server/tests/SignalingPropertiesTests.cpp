#include "Server/PropertiesSettings.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    PropertiesSettings settings(const std::filesystem::path &directory, const std::string &contents) {
        const auto path = directory / "test.properties";
        std::ofstream(path) << contents;
        return PropertiesSettings(path.string());
    }

    void checkDefaults() {
        PropertiesSettings defaults;
        require(defaults.getSignalingCertificate().empty(), "a certificate is set by default");
        require(defaults.getSignalingPrivateKey().empty(), "a private key is set by default");
        require(defaults.getSignalingExtraPort() == 0, "an extra signaling port is open by default");

        const std::string generated = PropertiesSettings::getDefaultContents();
        require(generated.find("signaling-certificate=\n") != std::string::npos,
                "generated properties omit signaling-certificate");
        require(generated.find("signaling-private-key=\n") != std::string::npos,
                "generated properties omit signaling-private-key");
        require(generated.find("signaling-extra-port=0\n") != std::string::npos,
                "generated properties omit signaling-extra-port");

        require(defaults.getSignalingAcmeDomain().empty(), "ACME is on by default");
        require(defaults.getSignalingAcmeDirectory() == "https://acme-v02.api.letsencrypt.org/directory",
                "the ACME directory is not the production one by default");
        require(defaults.getSignalingAcmeChallenge() == "tls-alpn-01", "the default challenge is not tls-alpn-01");
        require(generated.find("signaling-acme-domain=\n") != std::string::npos
                        && generated.find("signaling-acme-email=\n") != std::string::npos
                        && generated.find("signaling-acme-challenge=tls-alpn-01\n") != std::string::npos,
                "generated properties omit the ACME settings");
    }

    void checkValues(const std::filesystem::path &directory) {
        auto configured = settings(directory, "signaling-certificate=certs/fullchain.pem\n"
                                              "signaling-private-key=/etc/falcon/privkey.pem\n"
                                              "signaling-extra-port=443\n");
        require(configured.getSignalingCertificate() == "certs/fullchain.pem", "certificate path differs");
        require(configured.getSignalingPrivateKey() == "/etc/falcon/privkey.pem", "private key path differs");
        require(configured.getSignalingExtraPort() == 443, "extra signaling port differs");
        require(configured.getInvalidProperties().empty(), "valid signaling settings were reported");

        const std::string staging = "https://acme-staging-v02.api.letsencrypt.org/directory";
        auto issuing = settings(directory, "signaling-acme-domain=play.example.org\n"
                                           "signaling-acme-email=ops@example.org\n"
                                           "signaling-acme-directory="
                                                   + staging
                                                   + "\n"
                                                     "signaling-acme-challenge=http-01\n");
        require(issuing.getSignalingAcmeDomain() == "play.example.org"
                        && issuing.getSignalingAcmeEmail() == "ops@example.org",
                "ACME domain or email differs");
        require(issuing.getSignalingAcmeDirectory() == staging, "the staging directory was not kept");
        require(issuing.getSignalingAcmeChallenge() == "http-01" && issuing.getInvalidProperties().empty(),
                "http-01 was not accepted");

        auto dns = settings(directory, "signaling-acme-challenge=dns-01\n");
        require(dns.getSignalingAcmeChallenge() == "tls-alpn-01" && dns.getInvalidProperties().size() == 1,
                "an unsupported challenge was accepted");

        for (const std::string value: {"-1", "65536", "https", "4.43"}) {
            auto invalid = settings(directory, "signaling-extra-port=" + value + "\n");
            require(invalid.getSignalingExtraPort() == 0, "an invalid extra port did not fall back to none");
            require(invalid.getInvalidProperties().size() == 1, "an invalid extra port was not reported");
        }
    }
}

int main() {
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-signaling-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        checkDefaults();
        checkValues(directory);
    } catch (const std::exception &error) {
        std::clog << "Signaling properties regression failed: " << error.what() << '\n';
        result = 1;
    }
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
