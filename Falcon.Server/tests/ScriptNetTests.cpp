#include "Core/Archive/Gzip.h"
#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Player/ServerPlayer.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET TestSocket;
#define TEST_CLOSE closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int TestSocket;
#define TEST_CLOSE ::close
#endif

#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void writeFile(const std::filesystem::path &path, const std::string &contents) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << contents;
    }

    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &moduleUuid,
                   const std::string &script) {
        static int serial = 0;
        ++serial;
        const std::string header = "00000000-0000-4000-8000-" + std::to_string(300000000000 + serial);
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name + "\", \"uuid\": \"" + header
                          + "\", \"version\": [1, 0, 0], \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\", \"uuid\": \""
                          + moduleUuid
                          + "\", \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}], \"dependencies\": ["
                            "{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"},"
                            "{\"module_name\": \"@minecraft/server-admin\", \"version\": \"1.0.0-beta\"},"
                            "{\"module_name\": \"@minecraft/server-net\", \"version\": \"1.0.0-beta\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    // a tiny http and web socket peer on the loopback
    class TestServer {
    public:
        TestServer() {
#ifdef _WIN32
            WSADATA data;
            WSAStartup(MAKEWORD(2, 2), &data);
#endif
            mListener = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = 0;
            bind(mListener, (sockaddr *) &address, sizeof(address));
            listen(mListener, 16);
            socklen_t length = sizeof(address);
            getsockname(mListener, (sockaddr *) &address, &length);
            mPort = ntohs(address.sin_port);
            mThread = std::thread([this]() { _accept(); });
        }

        ~TestServer() {
            mStopping.store(true);
            // closing alone leaves a blocked accept waiting on linux
#ifdef _WIN32
            shutdown(mListener, SD_BOTH);
#else
            shutdown(mListener, SHUT_RDWR);
#endif
            TEST_CLOSE(mListener);
            if (mThread.joinable())
                mThread.join();
            for (std::thread &worker: mWorkers) {
                if (worker.joinable())
                    worker.join();
            }
        }

        int port() const {
            return mPort;
        }

    private:
        static bool readSome(TestSocket client, std::string &buffer) {
            char chunk[4096];
            const int read = (int) recv(client, chunk, sizeof(chunk), 0);
            if (read <= 0)
                return false;
            buffer.append(chunk, (size_t) read);
            return true;
        }

        static void sendAll(TestSocket client, const std::string &data) {
            size_t sent = 0;
            while (sent < data.size()) {
                const int written = (int) ::send(client, data.data() + sent, (int) (data.size() - sent), SEND_FLAGS);
                if (written <= 0)
                    return;
                sent += (size_t) written;
            }
        }

        static std::string header(const std::string &head, const std::string &name) {
            std::string lowered = head;
            for (char &c: lowered)
                c = (char) std::tolower((unsigned char) c);
            const size_t at = lowered.find("\r\n" + name + ":");
            if (at == std::string::npos)
                return std::string();
            const size_t start = at + 3 + name.size();
            const size_t end = head.find("\r\n", start);
            std::string value = head.substr(start, end - start);
            while (!value.empty() && value[0] == ' ')
                value.erase(0, 1);
            return value;
        }

        static void frame(TestSocket client, uint8_t opcode, const std::string &payload) {
            std::string out;
            out.push_back((char) (0x80 | opcode));
            out.push_back((char) payload.size());
            out += payload;
            sendAll(client, out);
        }

        void _accept() {
            for (;;) {
                TestSocket client = accept(mListener, nullptr, nullptr);
                if (mStopping.load()) {
                    if (client >= 0)
                        TEST_CLOSE(client);
                    return;
                }
                if (client < 0)
                    return;
                mWorkers.emplace_back([client]() { _serve(client); });
            }
        }

        static void _serve(TestSocket client) {
            std::string buffer;
            size_t end;
            while ((end = buffer.find("\r\n\r\n")) == std::string::npos) {
                if (!readSome(client, buffer)) {
                    TEST_CLOSE(client);
                    return;
                }
            }
            const std::string head = buffer.substr(0, end);
            std::string body = buffer.substr(end + 4);
            const size_t wanted = (size_t) std::atoi(header(head, "content-length").c_str());
            while (body.size() < wanted && readSome(client, body)) {
            }
            const std::string path = head.substr(head.find(' ') + 1,
                                                 head.find(' ', head.find(' ') + 1) - head.find(' ') - 1);

            if (path == "/ws" || path == "/ws-bye" || path == "/ws-big") {
                const std::string key = header(head, "sec-websocket-key") + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
                unsigned char digest[SHA_DIGEST_LENGTH];
                SHA1((const unsigned char *) key.data(), key.size(), digest);
                char accept[64] = {0};
                EVP_EncodeBlock((unsigned char *) accept, digest, sizeof(digest));
                sendAll(client, std::string("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                            "Connection: Upgrade\r\nSec-WebSocket-Accept: ")
                                        + accept + "\r\n\r\n");
                if (path == "/ws-big") {
                    // a 1 GiB text frame announced, a few bytes sent: the client must not wait to buffer it
                    std::string header("\x81\x7F", 2);
                    for (int shift = 56; shift >= 0; shift -= 8)
                        header.push_back((char) (((uint64_t) 1 << 30) >> shift));
                    sendAll(client, header + "yyyy");
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    TEST_CLOSE(client);
                    return;
                }
                if (path == "/ws-bye") {
                    frame(client, 0x1, "bye");
                    frame(client, 0x8, std::string("\x03\xE8", 2));
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    TEST_CLOSE(client);
                    return;
                }
                std::string pending;
                for (;;) {
                    while (pending.size() < 6) {
                        if (!readSome(client, pending)) {
                            TEST_CLOSE(client);
                            return;
                        }
                    }
                    const uint8_t opcode = (uint8_t) pending[0] & 0x0F;
                    const size_t size = (uint8_t) pending[1] & 0x7F;
                    while (pending.size() < 6 + size) {
                        if (!readSome(client, pending)) {
                            TEST_CLOSE(client);
                            return;
                        }
                    }
                    std::string payload = pending.substr(6, size);
                    for (size_t index = 0; index < payload.size(); ++index)
                        payload[index] = (char) (payload[index] ^ pending[2 + index % 4]);
                    pending.erase(0, 6 + size);
                    if (opcode == 0x8) {
                        frame(client, 0x8, payload);
                        TEST_CLOSE(client);
                        return;
                    }
                    frame(client, 0x1, "echo:" + payload);
                }
            }

            std::string type = "text/plain";
            std::string reply;
            if (path == "/slow") {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                reply = "late";
            } else if (path == "/structure") {
                Gzip::decompress(body, reply);
                type = "Application/X-Minecraft-NBT;  Type=structure";
            } else if (path == "/missing") {
                sendAll(client, "HTTP/1.1 404 Not Found\r\nContent-Length: 4\r\nConnection: close\r\n\r\nnope");
                TEST_CLOSE(client);
                return;
            } else {
                reply = head + "\n\n" + body;
            }
            sendAll(client, "HTTP/1.1 200 OK\r\nContent-Type: " + type
                                    + "\r\nX-Test: yes\r\nTransfer-Encoding: chunked"
                                      "\r\nConnection: close\r\n\r\n");
            std::string size(16, '\0');
            size.resize((size_t) std::snprintf(&size[0], size.size(), "%zx", reply.size()));
            sendAll(client, size + "\r\n" + reply + "\r\n0\r\n\r\n");
            TEST_CLOSE(client);
        }

        TestSocket mListener;
        int mPort = 0;
        std::atomic<bool> mStopping{false};
        std::thread mThread;
        std::vector<std::thread> mWorkers;
    };

    const char *const NET_UUID = "0000000b-0000-4000-9000-000000000001";
    const char *const TLS_UUID = "0000000b-0000-4000-9000-000000000002";

    const char *const NET = R"JS(
import { world, system } from "@minecraft/server";
import * as admin from "@minecraft/server-admin";
import * as net from "@minecraft/server-net";
const r = {};
const base = "http://127.0.0.1:" + admin.variables.get("port");
const ws = "ws://127.0.0.1:" + admin.variables.get("port");
const describe = (e) => e === undefined ? "undefined" : e.name + ": " + e.message;
const capture = (f) => {
    try {
        const value = f();
        return value === undefined ? "ok" : String(value);
    } catch (e) {
        return describe(e);
    }
};
const save = () => world.setDynamicProperty("net", JSON.stringify(r));
r.earlyConnect = capture(() => net.websocket.connect(ws + "/ws"));
r.earlyRequest = capture(() => new net.HttpRequest(base));
r.methods = Object.keys(net.HttpRequestMethod).join(",");
r.notFound = net.HttpStatusCode.NotFound;
r.packetId = net.PacketId.TextPacket;
r.reasons = JSON.stringify(net.WebSocketClientCloseReasons);
world.afterEvents.worldLoad.subscribe(async () => {
    try {
        const request = new net.HttpRequest(base + "/echo?x=1");
        r.defaults = request.method + "," + request.timeout + "," + JSON.stringify(request.body) + "," + request.headers.length;
        r.setBadMethod = capture(() => { request.method = "Fetch"; });
        request.setMethod(net.HttpRequestMethod.Post).setBody("hello").setTimeout(5)
            .addHeader("X-Session", "script").addHeader("Authorization", admin.secrets.get("token"))
            .addHeader("X-Own", new admin.SecretString("mine"));
        const echoed = await net.http.request(request);
        r.status = echoed.status;
        r.sameRequest = echoed.request === request;
        r.headerType = echoed.headers.every((h) => h instanceof net.HttpHeader);
        r.xTest = echoed.headers.find((h) => h.key === "X-Test").value;
        r.line = echoed.body.split("\r\n")[0];
        r.secret = echoed.body.includes("Authorization: hunter2");
        r.own = echoed.body.includes("X-Own: mine");
        r.session = echoed.body.includes("X-Session: config") && !echoed.body.includes("X-Session: script");
        r.payload = echoed.body.endsWith("\n\nhello");

        const missing = await net.http.get(base + "/missing");
        r.missing = missing.status + ":" + missing.body;

        r.notAllowed = await net.http.get("http://127.0.0.2:1/x").then(() => "resolved", (e) => describe(e) + "|" + e.uri);
        r.badScheme = await net.http.get("ftp://127.0.0.1/").then(() => "resolved", describe);
        r.noHost = await net.http.get("http:///path").then(() => "resolved", describe);
        r.tooLarge = await net.http.request(new net.HttpRequest(base + "/echo").setBody("x".repeat(5000)))
            .then(() => "resolved", (e) => describe(e) + "|" + e.maxBytes + "|" + e.providedBytes);

        const slow = net.http.get(base + "/slow");
        r.limit = await net.http.get(base + "/echo").then(() => "resolved",
            (e) => describe(e) + "|" + e.inFlightRequests + "|" + e.maxConcurrentRequests);
        net.http.cancelAll("stop");
        r.cancelled = await slow.then(() => "resolved", (e) => describe(e) + "|" + (e instanceof Error));

        const structure = world.structureManager.createEmpty("test:source", { x: 2, y: 1, z: 3 });
        const uploaded = await net.http.request(new net.HttpRequest(base + "/structure").setMethod(net.HttpRequestMethod.Put)
            .setBody(structure));
        const copy = uploaded.deserialize("test:copy");
        r.deserialized = copy.id + ":" + copy.size.x + "," + copy.size.y + "," + copy.size.z;
        r.again = capture(() => uploaded.deserialize("test:copy"));
        r.wrongType = capture(() => missing.deserialize("test:other"));
        r.conflict = await net.http.request(new net.HttpRequest(base + "/structure").setBody(structure)
            .addHeader("Content-Type", "text/plain")).then(() => "resolved", describe);

        const client = await net.websocket.connect(ws + "/ws", [new net.HttpHeader("X-Hello", "1")]);
        r.open = client.isOpen && client instanceof net.WebSocketClient;
        r.wsLimit = await net.websocket.connect(ws + "/ws").then(() => "resolved",
            (e) => describe(e) + "|" + e.connectedSockets + "|" + e.maxConcurrentConnections);
        r.wsTooLarge = capture(() => client.send("x".repeat(5000)));
        const echo = await new Promise((resolve) => {
            client.afterEvents.message.subscribe((event) => resolve(event.message));
            client.send("ping");
        });
        r.echo = echo;
        const closed = await new Promise((resolve) => {
            client.afterEvents.close.subscribe((event) => resolve(event.reason));
            client.close();
        });
        r.closedByClient = closed + "," + client.isOpen;
        r.sendClosed = capture(() => client.send("late"));
        r.closeClosed = capture(() => client.close());

        const bye = await net.websocket.connect(ws + "/ws-bye");
        const byeEvents = await new Promise((resolve) => {
            const seen = [];
            bye.afterEvents.message.subscribe((event) => seen.push(event.message));
            bye.afterEvents.close.subscribe((event) => resolve(seen.join("+") + "|" + event.reason));
        });
        r.closedByServer = byeEvents;
        const big = await net.websocket.connect(ws + "/ws-big");
        r.closedTooLarge = await new Promise((resolve) => {
            const seen = [];
            big.afterEvents.message.subscribe((event) => seen.push(event.message));
            big.afterEvents.close.subscribe((event) => resolve(seen.length + "|" + event.reason));
        });
        r.refused = await net.websocket.connect("ws://127.0.0.1:1/ws").then(() => "resolved", (e) => e.name);
    } catch (e) {
        r.failure = describe(e) + " " + e.stack;
    }
    r.done = true;
    save();
});
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "net:packets")
        return;
    net.beforeEvents.packetSend.subscribe((event) => {
        r.sent = (r.sent || "") + event.packetId + ":" + event.recipients.length + ":"
            + (event.recipients[0] === undefined ? "none" : event.recipients[0].name) + ";";
        event.cancel = true;
        save();
    }, { monitoredPacketIds: [net.PacketId.TextPacket] });
    net.beforeEvents.packetReceive.subscribe((event) => {
        r.received = (r.received || "") + event.packetId + ":" + event.packetSize + ":"
            + (event.sender === undefined ? "none" : event.sender.name) + ";";
        r.restricted = capture(() => net.websocket.connect(ws + "/ws"));
        save();
    }, { ignoredPacketIds: [net.PacketId.MovePlayerPacket] });
    r.badOptions = capture(() => net.beforeEvents.packetSend.subscribe(() => {}, { monitoredPacketIds: ["Nope"] }));
    save();
});
)JS";

    const char *const TLS = R"JS(
import { world } from "@minecraft/server";
import { http } from "@minecraft/server-net";
world.afterEvents.worldLoad.subscribe(async () => {
    const result = await http.get("http://127.0.0.1:1/").then(() => "resolved", (e) => e.name + ": " + e.message + "|" + e.uri);
    world.setDynamicProperty("tls", result);
});
)JS";

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        std::string actual = value->string();
        if (value->mType == json::Value::Type::Boolean)
            actual = value->mBoolean ? "true" : "false";
        if (value->mType == json::Value::Type::Number)
            actual = std::to_string((long long) value->mNumber);
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    std::string raw(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("net");
        return it == owner.getWorldDynamicProperties().end() ? std::string("nothing") : it->second.mString;
    }

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("net");
        if (it == owner.getWorldDynamicProperties().end())
            return nullptr;
        return json::parse(it->second.mString);
    }

    void run(const std::filesystem::path &directory) {
        TestServer server;
        const std::string port = std::to_string(server.port());
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "net", NET_UUID, NET);
        writePack(packs, "tls", TLS_UUID, TLS);
        writeFile(
                directory / "config" / NET_UUID / "permissions.json",
                "{\"allowed_modules\": [\"*\"], \"module_permissions\": {\"@minecraft/server-net\": {"
                "\"allowed_uris\": [\"http://127.0.0.1:"
                        + port + "/\", \"ws://127.0.0.1:" + port
                        + "\", \"ws://127.0.0.1:1\", \"ftp://127.0.0.1\"],"
                          " \"max_body_bytes\": 4096, \"max_concurrent_requests\": 1, \"max_websocket_connections\": 1,"
                          " \"max_websocket_payload_size\": 64,"
                          " \"session_headers\": {\"X-Session\": \"config\"}}}}");
        writeFile(directory / "config" / NET_UUID / "variables.json", "{\"port\": " + port + "}");
        writeFile(directory / "config" / NET_UUID / "secrets.json", R"({"token": "hunter2"})");
        // the game reads only whether force_tls is present
        writeFile(
                directory / "config" / TLS_UUID / "permissions.json",
                R"({"allowed_modules": ["*"], "module_permissions": {"@minecraft/server-net": {"force_https": false}}})");

        ServerNetworkHandler owner("Script net", "", 2);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        require(engine.loadPacks(manager.getPacks()) == 2, "the net packs did not start");

        NetworkIdentifier guest;
        ServerPlayer &player = owner.getPlayers().try_emplace(guest, guest, 3001).first->second;
        player.setName("Guest");
        engine.startup();

        std::unique_ptr<json::Value> values;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        int64_t tick = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            engine.tick(tick++);
            values = result(owner);
            const auto tls = owner.getWorldDynamicProperties().find("tls");
            if (values != nullptr && values->get("done") != nullptr && tls != owner.getWorldDynamicProperties().end())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        require(values != nullptr && values->get("done") != nullptr, "the net pack did not finish: " + raw(owner));
        if (const json::Value *failure = values->get("failure"))
            throw std::runtime_error("the net pack failed: " + failure->string());

        const auto tls = owner.getWorldDynamicProperties().find("tls");
        require(tls != owner.getWorldDynamicProperties().end(), "the tls pack did not finish");
        require(tls->second.mString
                        == "TLSOnlyError: HTTPS is required for HTTP requests and WSS is required for Web Sockets."
                           "|http://127.0.0.1:1/",
                "force_https false: " + tls->second.mString);

        engine.emitScriptEvent("net:packets", "");
        require(engine.beforePacketSend({guest}, MinecraftPacketIds::Text), "a cancelled packetSend went out");
        require(!engine.beforePacketSend({guest}, MinecraftPacketIds::SetTime), "an unmonitored packet was cancelled");
        NetworkIdentifier stranger(std::string("10.0.0.7"), 19132);
        engine.beforePacketSend({stranger}, MinecraftPacketIds::Text);
        require(!engine.beforePacketReceive(guest, MinecraftPacketIds::Text, 42), "packetReceive cancelled itself");
        engine.beforePacketReceive(guest, MinecraftPacketIds::MovePlayer, 10);
        require(!engine.beforePacketReceive(guest, MinecraftPacketIds::KeepAlive, 3), "an unlisted packet fired");
        values = result(owner);

        expect(*values, "earlyConnect",
               "ReferenceError: Native function [WebSocket::connect] cannot be used in early execution.");
        expect(*values, "earlyRequest",
               "ReferenceError: Native constructor for [HttpRequest::constructor] cannot be used in early execution.");
        expect(*values, "methods", "Delete,Get,Head,Patch,Post,Put");
        expect(*values, "notFound", "404");
        expect(*values, "packetId", "TextPacket");
        expect(*values, "reasons", R"({"ClosedByClient":1,"ClosedByServer":0,"IncomingPayloadsTooLarge":2})");
        expect(*values, "defaults", "Get,0,\"\",0");
        expect(*values, "setBadMethod", "TypeError: Native type conversion failed.");
        expect(*values, "status", "200");
        expect(*values, "sameRequest", "true");
        expect(*values, "headerType", "true");
        expect(*values, "xTest", "yes");
        expect(*values, "line", "POST /echo?x=1 HTTP/1.1");
        expect(*values, "secret", "true");
        expect(*values, "own", "true");
        expect(*values, "session", "true");
        expect(*values, "payload", "true");
        expect(*values, "missing", "404:nope");
        expect(*values, "notAllowed",
               "UriNotAllowedError: Request URI is not permitted by configuration.|http://127.0.0.2:1/x");
        expect(*values, "badScheme",
               "MalformedUriError: Supported URI schemes are 'https' and 'http', the value 'ftp' provided is invalid.");
        expect(*values, "noHost", "MalformedUriError: No valid hostname was found in provided URI 'http:///path'.");
        expect(*values, "tooLarge",
               "RequestBodyTooLargeError: HTTP request or Web Socket message body exceeds allowed size.|4096|5000");
        expect(*values, "limit", "HttpRequestLimitExceededError: Maximum concurrent HTTP requests exceeded.|1|1");
        expect(*values, "cancelled", "Error: stop|true");
        expect(*values, "deserialized", "test:copy:2,1,3");
        expect(*values, "again", "SerializableParseError: A structure named 'test:copy' already exists.");
        expect(*values, "wrongType", "SerializableParseError: No deserializer registered for Content-Type: .");
        expect(*values, "conflict",
               "SerializableParseError: Content-Type and Content-Encoding headers must not be set when the request body "
               "is a serializable object. They are determined automatically by the serializer "
               "(application/x-minecraft-nbt; type=structure).");
        expect(*values, "open", "true");
        expect(*values, "wsLimit",
               "WebSocketLimitExceededError: Maximum concurrent web socket connections exceeded.|1|1");
        expect(*values, "wsTooLarge",
               "RequestBodyTooLargeError: HTTP request or Web Socket message body exceeds allowed size.");
        expect(*values, "echo", "echo:ping");
        expect(*values, "closedByClient", "1,false");
        expect(*values, "sendClosed", "WebSocketNotConnectedError: This web socket is not connected.");
        expect(*values, "closeClosed", "WebSocketNotConnectedError: This web socket is not connected.");
        expect(*values, "closedByServer", "bye|0");
        expect(*values, "closedTooLarge", "0|2");
        expect(*values, "refused", "InternalWebSocketError");
        expect(*values, "sent", "TextPacket:1:Guest;TextPacket:1:none;");
        expect(*values, "received", "TextPacket:42:Guest;");
        expect(*values, "restricted",
               "ReferenceError: Native function [WebSocket::connect] cannot be used in restricted execution.");
        expect(*values, "badOptions", "TypeError: Native type conversion failed.");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-net-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script net regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
