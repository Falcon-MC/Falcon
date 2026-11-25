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

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
typedef SOCKET TestSocket;
#define TEST_CLOSE_SOCKET ::closesocket
#else
typedef int TestSocket;
#define TEST_CLOSE_SOCKET ::close
#endif

namespace {
    typedef std::chrono::steady_clock Clock;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    long long elapsedMs(Clock::time_point begin) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - begin).count();
    }

    TestSocket connectLocal(unsigned short port) {
        TestSocket descriptor = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
        require(descriptor != INVALID_SOCKET, "socket failed");
        const DWORD timeout = 10000;
#else
        require(descriptor >= 0, "socket failed");
        timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
#endif
        setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof(timeout));
#ifdef SO_NOSIGPIPE
        // the client side writes after the server closed; the server side must survive that on its own
        int noSignal = 1;
        setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        require(connect(descriptor, (sockaddr *) &address, sizeof(address)) == 0, "connect failed");
        return descriptor;
    }

#ifdef MSG_NOSIGNAL
    const int SEND_FLAGS = MSG_NOSIGNAL;
#else
    const int SEND_FLAGS = 0;
#endif

    void sendText(TestSocket descriptor, const std::string &text) {
        send(descriptor, text.data(), (int) text.size(), SEND_FLAGS);
    }

    std::string readAll(TestSocket descriptor) {
        std::string response;
        char chunk[4096];
        int read;
        while ((read = (int) recv(descriptor, chunk, sizeof(chunk), 0)) > 0)
            response.append(chunk, (size_t) read);
        return response;
    }

    std::string request(unsigned short port) {
        const TestSocket descriptor = connectLocal(port);
        sendText(descriptor, "GET /v1/join HTTP/1.1\r\nHost: x\r\n\r\n");
        const std::string response = readAll(descriptor);
        TEST_CLOSE_SOCKET(descriptor);
        return response;
    }

    bool answerOffer(const std::string &, const std::string &, std::string &answer, int &) {
        answer = "v=0";
        return true;
    }

    void startServer(nethernet::SignalingServer &server) {
        server.setStatusProvider([]() {
            return std::string("{}");
        });
        require(server.start("127.0.0.1", 0, answerOffer), "the signaling server did not start");
    }

    void closesAPartialRequestAtTheDeadline() {
        nethernet::SignalingServer server;
        startServer(server);

        const Clock::time_point begin = Clock::now();
        const TestSocket descriptor = connectLocal(server.getBoundPort());
        sendText(descriptor, "GET /v1/jo");
        readAll(descriptor);
        const long long elapsed = elapsedMs(begin);
        TEST_CLOSE_SOCKET(descriptor);

        require(elapsed >= 4000 && elapsed < 7000, "a partial request was held " + std::to_string(elapsed) + " ms");
        require(request(server.getBoundPort()).rfind("HTTP/1.1 200", 0) == 0, "the server stopped answering");
        server.stop();
    }

    void closesATricklingRequestAtTheDeadline() {
        nethernet::SignalingServer server;
        startServer(server);

        const std::string text = "GET /v1/join HTTP/1.1\r\nHost: a-very-long-host-name.example.org\r\n\r\n";
        const Clock::time_point begin = Clock::now();
        const TestSocket descriptor = connectLocal(server.getBoundPort());
        bool closed = false;
        for (char character: text) {
            if (send(descriptor, &character, 1, SEND_FLAGS) != 1) {
                closed = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (elapsedMs(begin) > 8000)
                break;
        }
        readAll(descriptor);
        const long long elapsed = elapsedMs(begin);
        TEST_CLOSE_SOCKET(descriptor);

        require(closed || elapsed < 8000, "a trickling request was held past the deadline");
        require(elapsed < 8000, "a trickling request was held " + std::to_string(elapsed) + " ms");
        server.stop();
    }

    void capsConnectionsPerAddress() {
        nethernet::SignalingServer server;
        startServer(server);

        std::vector<TestSocket> idle;
        for (int index = 0; index < 8; ++index)
            idle.push_back(connectLocal(server.getBoundPort()));
        // the accept loop must have seen all eight before the next one
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        const Clock::time_point begin = Clock::now();
        const std::string refused = request(server.getBoundPort());
        require(refused.rfind("HTTP/1.1 503", 0) == 0, "a ninth connection from one address was served");
        require(elapsedMs(begin) < 1000, "the busy answer was slow");

        for (TestSocket descriptor: idle)
            TEST_CLOSE_SOCKET(descriptor);

        // closed sockets end their requests at once and give the room back
        bool served = false;
        for (int attempt = 0; attempt < 20 && !served; ++attempt) {
            served = request(server.getBoundPort()).rfind("HTTP/1.1 200", 0) == 0;
            if (!served)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        require(served, "the address stayed blocked after its sockets closed");
        server.stop();
    }

    void survivesAPeerThatLeavesBeforeTheAnswer() {
        nethernet::SignalingServer server;
        startServer(server);

        for (int index = 0; index < 20; ++index) {
            const TestSocket descriptor = connectLocal(server.getBoundPort());
            sendText(descriptor, "GET /v1/join HTTP/1.1\r\nHost: x\r\n\r\n");
            TEST_CLOSE_SOCKET(descriptor);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        require(request(server.getBoundPort()).rfind("HTTP/1.1 200", 0) == 0, "the server stopped answering");
        server.stop();
    }
}

int main() {
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif

    try {
        require(nethernet::SignalingServer().getBoundPort() == 0, "a fresh server has a port");
        survivesAPeerThatLeavesBeforeTheAnswer();
        capsConnectionsPerAddress();
        closesAPartialRequestAtTheDeadline();
        closesATricklingRequestAtTheDeadline();
    } catch (const std::exception &error) {
        std::cout << "FAIL: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "PASS: signaling deadline" << std::endl;
    return 0;
}
