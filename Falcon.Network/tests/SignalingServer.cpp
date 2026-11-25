#include "Network/Acme/AcmeChallengeStore.h"
#include "Network/Acme/AcmeCrypto.h"
#include "Network/NetherNet/NetherNetSignalingServer.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
typedef SOCKET TestSocket;
#define TEST_CLOSE_SOCKET ::closesocket
#else
typedef int TestSocket;
#define TEST_CLOSE_SOCKET ::close
#endif

namespace {
    const char *STATUS_JSON = "{\"status\":\"ok\"}";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    TestSocket connectLocal(unsigned short port) {
        TestSocket descriptor = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
        require(descriptor != INVALID_SOCKET, "socket failed");
#else
        require(descriptor >= 0, "socket failed");
#endif

#ifdef _WIN32
        const DWORD timeout = 5000;
#else
        timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
#endif
        setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof(timeout));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        require(connect(descriptor, (sockaddr *) &address, sizeof(address)) == 0, "connect failed");
        return descriptor;
    }

    std::string plainRequest(unsigned short port, const std::string &request) {
        const TestSocket descriptor = connectLocal(port);
        send(descriptor, request.data(), (int) request.size(), 0);

        std::string response;
        char chunk[4096];
        int read;
        while ((read = (int) recv(descriptor, chunk, sizeof(chunk), 0)) > 0)
            response.append(chunk, (size_t) read);

        TEST_CLOSE_SOCKET(descriptor);
        return response;
    }

    // returns false when the handshake is refused; the refusal must carry the alert, not a reset
    bool tlsRequest(unsigned short port, const std::string &request, std::string &response, bool &alerted) {
        SSL_CTX *context = SSL_CTX_new(TLS_client_method());
        require(context != nullptr, "client context failed");
        SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);

        const TestSocket descriptor = connectLocal(port);
        SSL *ssl = SSL_new(context);
        SSL_set_fd(ssl, (int) descriptor);
        SSL_set_tlsext_host_name(ssl, "localhost");

        alerted = false;
        bool connected = SSL_connect(ssl) == 1;
        if (connected) {
            SSL_write(ssl, request.data(), (int) request.size());
            char chunk[4096];
            int read;
            while ((read = SSL_read(ssl, chunk, sizeof(chunk))) > 0)
                response.append(chunk, (size_t) read);
            SSL_shutdown(ssl);
        } else {
            const unsigned long error = ERR_peek_last_error();
            alerted = ERR_GET_REASON(error) == SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE;
            ERR_clear_error();
        }

        SSL_free(ssl);
        TEST_CLOSE_SOCKET(descriptor);
        SSL_CTX_free(context);
        return connected;
    }

    void writeSelfSignedCertificate(const std::string &certificatePath, const std::string &keyPath,
                                    const std::string &commonName = "localhost") {
        EVP_PKEY *key = nullptr;
        EVP_PKEY_CTX *keyContext = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
        require(keyContext != nullptr && EVP_PKEY_keygen_init(keyContext) == 1
                && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(keyContext, NID_X9_62_prime256v1) == 1
                && EVP_PKEY_keygen(keyContext, &key) == 1, "key generation failed");
        EVP_PKEY_CTX_free(keyContext);

        X509 *certificate = X509_new();
        ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
        X509_gmtime_adj(X509_getm_notBefore(certificate), 0);
        X509_gmtime_adj(X509_getm_notAfter(certificate), 3600);
        X509_set_pubkey(certificate, key);
        // OpenSSL 4 hands out the subject read-only: build the name apart
        X509_NAME *name = X509_NAME_new();
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *) commonName.c_str(), -1, -1, 0);
        X509_set_subject_name(certificate, name);
        X509_set_issuer_name(certificate, name);
        X509_NAME_free(name);
        require(X509_sign(certificate, key, EVP_sha256()) > 0, "certificate signing failed");

        FILE *file = std::fopen(certificatePath.c_str(), "wb");
        require(file != nullptr, "could not write the certificate");
        PEM_write_X509(file, certificate);
        std::fclose(file);

        file = std::fopen(keyPath.c_str(), "wb");
        require(file != nullptr, "could not write the key");
        PEM_write_PrivateKey(file, key, nullptr, nullptr, 0, nullptr, nullptr);
        std::fclose(file);

        X509_free(certificate);
        EVP_PKEY_free(key);
    }

    bool answerOffer(const std::string &networkID, const std::string &offer, std::string &answer, int &) {
        answer = "answer:" + networkID + ":" + offer;
        return true;
    }

    void startServer(nethernet::SignalingServer &server) {
        server.setStatusProvider([]() {
            return std::string(STATUS_JSON);
        });
        require(server.start("127.0.0.1", 0, answerOffer), "the signaling server did not start");
        require(server.getBoundPort() != 0, "no bound port");
    }

    void servesPlainHttpWithoutCertificate() {
        nethernet::SignalingServer server;
        startServer(server);
        require(!server.isSecure(), "secure without a certificate");

        const std::string get = plainRequest(server.getBoundPort(), "GET /v1/join HTTP/1.1\r\nHost: x\r\n\r\n");
        require(get.rfind("HTTP/1.1 200", 0) == 0, "plain GET /v1/join is not 2xx");
        require(get.find(STATUS_JSON) != std::string::npos, "plain GET /v1/join lost the status body");

        const std::string head = plainRequest(server.getBoundPort(), "HEAD /v1/join HTTP/1.1\r\nHost: x\r\n\r\n");
        require(head.rfind("HTTP/1.1 200", 0) == 0, "HEAD /v1/join is not 2xx");
        require(head.find(STATUS_JSON) == std::string::npos, "HEAD /v1/join carried a body");

        const std::string offer = "v=0";
        const std::string post = plainRequest(server.getBoundPort(),
                                              "POST /v1/join/42 HTTP/1.1\r\nHost: x\r\nContent-Type: application/sdp\r\n"
                                              "Content-Length: " + std::to_string(offer.size()) + "\r\n\r\n" + offer);
        require(post.rfind("HTTP/1.1 200", 0) == 0, "POST /v1/join/{id} is not 2xx");
        require(post.find("answer:42:v=0") != std::string::npos, "POST /v1/join/{id} lost the answer");

        server.stop();
    }

    void refusesTlsAtOnceWithoutCertificate() {
        nethernet::SignalingServer server;
        startServer(server);

        // the game waits for every https probe before trying http: a refusal must not wait for a timeout
        const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        std::string response;
        bool alerted = false;
        const bool connected = tlsRequest(server.getBoundPort(), "GET /v1/join HTTP/1.1\r\n\r\n", response, alerted);
        const long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin).count();

        require(!connected, "a TLS handshake succeeded without a certificate");
        require(alerted, "the TLS refusal carried no handshake_failure alert");
        require(elapsed < 1000, "the TLS refusal took " + std::to_string(elapsed) + " ms");
        std::cout << "tls refusal: " << elapsed << " ms" << std::endl;

        server.stop();
    }

    void servesHttpsAndHttpWithCertificate(const std::filesystem::path &directory) {
        const std::string certificate = (directory / "chain.pem").string();
        const std::string key = (directory / "key.pem").string();
        writeSelfSignedCertificate(certificate, key);

        nethernet::SignalingServer server;
        server.setCertificate(certificate, key);
        startServer(server);
        require(server.isSecure(), "the certificate was not loaded");

        const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        std::string response;
        bool alerted = false;
        require(tlsRequest(server.getBoundPort(), "GET /v1/join HTTP/1.1\r\nHost: localhost\r\n\r\n", response,
                           alerted), "the TLS handshake failed with a certificate");
        const long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin).count();
        require(response.rfind("HTTP/1.1 200", 0) == 0, "https GET /v1/join is not 2xx");
        require(response.find(STATUS_JSON) != std::string::npos, "https GET /v1/join lost the status body");
        std::cout << "https probe: " << elapsed << " ms" << std::endl;

        // same port, plaintext: the game's http batch after a failed certificate check
        const std::string plain = plainRequest(server.getBoundPort(), "GET /v1/join HTTP/1.1\r\nHost: x\r\n\r\n");
        require(plain.rfind("HTTP/1.1 200", 0) == 0, "plain GET /v1/join stopped working beside TLS");

        server.stop();
    }

    void keepsPlainHttpWhenTheCertificateIsMissing(const std::filesystem::path &directory) {
        nethernet::SignalingServer server;
        server.setCertificate((directory / "missing.pem").string(), (directory / "missing.key").string());
        startServer(server);
        require(!server.isSecure(), "secure with a missing certificate");

        const std::string plain = plainRequest(server.getBoundPort(), "GET /v1/join HTTP/1.1\r\nHost: x\r\n\r\n");
        require(plain.rfind("HTTP/1.1 200", 0) == 0, "a missing certificate broke plain http");

        server.stop();
    }

    struct TlsProbe {
        bool mConnected = false;
        std::string mCommonName;
        std::string mProtocol;
    };

    TlsProbe probeTls(unsigned short port, const std::string &serverName, bool offerAcme) {
        TlsProbe probe;
        SSL_CTX *context = SSL_CTX_new(TLS_client_method());
        require(context != nullptr, "client context failed");
        SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);

        const TestSocket descriptor = connectLocal(port);
        SSL *ssl = SSL_new(context);
        SSL_set_fd(ssl, (int) descriptor);
        SSL_set_tlsext_host_name(ssl, serverName.c_str());
        if (offerAcme) {
            static const unsigned char ACME[] = {10, 'a', 'c', 'm', 'e', '-', 't', 'l', 's', '/', '1'};
            SSL_set_alpn_protos(ssl, ACME, sizeof(ACME));
        }

        probe.mConnected = SSL_connect(ssl) == 1;
        if (probe.mConnected) {
            const unsigned char *protocol = nullptr;
            unsigned int length = 0;
            SSL_get0_alpn_selected(ssl, &protocol, &length);
            probe.mProtocol.assign((const char *) protocol, length);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            X509 *peer = SSL_get1_peer_certificate(ssl);
#else
            X509 *peer = SSL_get_peer_certificate(ssl);
#endif
            char name[256] = {0};
            if (peer != nullptr)
                X509_NAME_get_text_by_NID(X509_get_subject_name(peer), NID_commonName, name, sizeof(name));
            probe.mCommonName = name;
            X509_free(peer);
            SSL_shutdown(ssl);
        }
        ERR_clear_error();

        SSL_free(ssl);
        TEST_CLOSE_SOCKET(descriptor);
        SSL_CTX_free(context);
        return probe;
    }

    void answersAcmeChallengesBesideTheJoin() {
        const std::string domain = "play.example.org";
        std::string challengeCertificate;
        std::string challengeKey;
        require(acme::createTlsAlpnCertificate(domain, "token.thumb", challengeCertificate, challengeKey),
                "no tls-alpn-01 certificate");

        const std::shared_ptr<acme::ChallengeStore> store = std::make_shared<acme::ChallengeStore>();
        store->putHttp("abc_-1", "abc_-1.thumb");
        store->putTlsAlpn(domain, challengeCertificate, challengeKey);

        nethernet::SignalingServer server;
        server.setChallengeStore(store);
        startServer(server);
        require(!server.isSecure(), "a challenge-only context counts as a certificate");

        const std::string http = plainRequest(server.getBoundPort(),
                                              "GET /.well-known/acme-challenge/abc_-1 HTTP/1.1\r\nHost: x\r\n\r\n");
        require(http.rfind("HTTP/1.1 200", 0) == 0 && http.find("\r\n\r\nabc_-1.thumb") != std::string::npos,
                "the http-01 answer is wrong");
        const std::string unknown = plainRequest(server.getBoundPort(),
                                                 "GET /.well-known/acme-challenge/nope HTTP/1.1\r\nHost: x\r\n\r\n");
        require(unknown.rfind("HTTP/1.1 404", 0) == 0, "an unknown token was answered");

        const std::string join = plainRequest(server.getBoundPort(), "GET /v1/join HTTP/1.1\r\nHost: x\r\n\r\n");
        require(join.rfind("HTTP/1.1 200", 0) == 0, "ACME blocked /v1/join");

        const TlsProbe answered = probeTls(server.getBoundPort(), domain, true);
        require(answered.mConnected && answered.mProtocol == "acme-tls/1" && answered.mCommonName == domain,
                "the tls-alpn-01 handshake did not present the challenge certificate");

        require(!probeTls(server.getBoundPort(), "other.example.org", true).mConnected,
                "a tls-alpn-01 hello for an unknown name was answered");
        require(!probeTls(server.getBoundPort(), domain, false).mConnected,
                "a plain TLS hello got the challenge certificate");

        server.stop();
    }

    void reloadsTheCertificateWhileRunning(const std::filesystem::path &directory) {
        const std::string firstCertificate = (directory / "first.pem").string();
        const std::string firstKey = (directory / "first.key").string();
        const std::string secondCertificate = (directory / "second.pem").string();
        const std::string secondKey = (directory / "second.key").string();
        writeSelfSignedCertificate(firstCertificate, firstKey, "first.test");
        writeSelfSignedCertificate(secondCertificate, secondKey, "second.test");

        nethernet::SignalingServer server;
        server.setCertificate(firstCertificate, firstKey);
        startServer(server);
        require(probeTls(server.getBoundPort(), "first.test", false).mCommonName == "first.test",
                "the first certificate is not served");

        require(!server.reloadCertificate((directory / "missing.pem").string(), secondKey),
                "a missing certificate was installed");
        require(probeTls(server.getBoundPort(), "first.test", false).mCommonName == "first.test",
                "a failed reload dropped the served certificate");

        require(server.reloadCertificate(secondCertificate, secondKey), "the reload failed");
        require(server.isSecure(), "the reloaded server is not secure");
        require(probeTls(server.getBoundPort(), "second.test", false).mCommonName == "second.test",
                "the reloaded certificate is not served");

        server.stop();
    }

    void upgradesFromPlainHttpAtRuntime(const std::filesystem::path &directory) {
        const std::string certificate = (directory / "late.pem").string();
        const std::string key = (directory / "late.key").string();
        writeSelfSignedCertificate(certificate, key, "late.test");

        nethernet::SignalingServer server;
        startServer(server);
        require(!probeTls(server.getBoundPort(), "late.test", false).mConnected, "TLS before any certificate");
        require(server.reloadCertificate(certificate, key) && server.isSecure(), "the first certificate failed");
        require(probeTls(server.getBoundPort(), "late.test", false).mCommonName == "late.test",
                "a certificate issued after start is not served");
        server.stop();
    }
}

int main() {
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif

    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "falcon-signaling-test";
    std::filesystem::create_directories(directory);

    try {
        servesPlainHttpWithoutCertificate();
        refusesTlsAtOnceWithoutCertificate();
        servesHttpsAndHttpWithCertificate(directory);
        keepsPlainHttpWhenTheCertificateIsMissing(directory);
        answersAcmeChallengesBesideTheJoin();
        reloadsTheCertificateWhileRunning(directory);
        upgradesFromPlainHttpAtRuntime(directory);
    } catch (const std::exception &error) {
        std::cout << "FAIL: " << error.what() << std::endl;
        std::filesystem::remove_all(directory);
        return 1;
    }

    std::filesystem::remove_all(directory);
    std::cout << "PASS: signaling server" << std::endl;
    return 0;
}
