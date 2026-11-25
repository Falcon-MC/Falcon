#include "Network/Client/ClientNetworkSystem.h"
#include "Core/Json/Json.h"
#include "Protocol/Packets/PacketViolationWarningPacket.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/Auth/MinecraftAuthentication.h"
#include "Network/Client/NetherNetClient.h"
#include "Network/Client/RakNetClient.h"
#include "Network/Crypto/EncryptionHandshake.h"
#include "Network/Crypto/KeyPair.h"
#include "Network/Http/HttpClient.h"
#include "Network/NetherNet/NetherNetDiscovery.h"
#include "Network/NetherNet/NetherNetHttpSignaling.h"
#include "Network/NetherNet/NetherNetJsonRpcSignaling.h"
#include "Network/NetherNet/NetherNetWebSocketSignaling.h"
#include "Protocol/Packets/ChunkRadiusUpdatedPacket.h"
#include "Protocol/Packets/ClientCacheStatusPacket.h"
#include "Protocol/Packets/ClientToServerHandshakePacket.h"
#include "Protocol/Packets/ItemRegistryPacket.h"
#include "Protocol/Packets/LoginPacket.h"
#include "Protocol/Packets/NetworkSettingsPacket.h"
#include "Protocol/Packets/PlayStatusPacket.h"
#include "Protocol/Packets/RequestChunkRadiusPacket.h"
#include "Protocol/Packets/RequestNetworkSettingsPacket.h"
#include "Protocol/Packets/ResourcePackChunkDataPacket.h"
#include "Protocol/Packets/ResourcePackChunkRequestPacket.h"
#include "Protocol/Packets/ResourcePackClientResponsePacket.h"
#include "Protocol/Packets/ResourcePackDataInfoPacket.h"
#include "Protocol/Packets/ResourcePackStackPacket.h"
#include "Protocol/Packets/ResourcePacksInfoPacket.h"
#include "Protocol/Packets/ServerToClientHandshakePacket.h"
#include "Protocol/Packets/ServerboundLoadingScreenPacket.h"
#include "Protocol/Packets/SetLocalPlayerAsInitializedPacket.h"
#include "Protocol/Packets/StartGamePacket.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <thread>
#include <utility>
#include <vector>
#include <future>
#include <deque>
#include <openssl/sha.h>

namespace {

    const int IDLE_WAIT_MS = 2;
    const int CDN_TIMEOUT_MS = 120000;
    const uint64_t MAX_PACK_SIZE = 512ull * 1024 * 1024;
    const uint64_t MAX_STACK_SIZE = 512ull * 1024 * 1024;
    const size_t MAX_PACK_COUNT = 32;
    const size_t MAX_CDN_PACKET_BYTES = 64ull * 1024 * 1024;
    const int COMPRESSION_NONE_ID = 0xffff;

    std::string redirectUrl(const std::string &base, const std::string &location) {
        if (location.empty() || location.find_first_of("\r\n") != std::string::npos) return {};
        if (location.rfind("https://", 0) == 0 || location.rfind("http://", 0) == 0) return location;
        if (location.rfind("//", 0) == 0) return base.substr(0, base.find(':') + 1) + location;
        if (location.substr(0, location.find_first_of("/?#")).find(':') != std::string::npos) return {};
        size_t authorityEnd = base.find_first_of("/?#", base.find("://") + 3);
        std::string origin = base.substr(0, authorityEnd);
        std::string path = authorityEnd == std::string::npos ? "/" : base.substr(authorityEnd);
        if (path.front() != '/') path.insert(path.begin(), '/');
        path = path.substr(0, path.find('#'));
        if (location.front() == '#') return origin + path;
        path = path.substr(0, path.find('?'));
        if (location.front() == '?') return origin + path + location;
        path = location.front() == '/' ? location : path.substr(0, path.rfind('/') + 1) + location;
        std::string suffix;
        size_t query = path.find_first_of("?#");
        if (query != std::string::npos) { suffix = path.substr(query); path.resize(query); }
        std::vector<std::string> segments;
        for (size_t start = 1; start <= path.size();) {
            size_t end = path.find('/', start);
            std::string part = path.substr(start, end == std::string::npos ? end : end - start);
            if (part == "..") { if (!segments.empty()) segments.pop_back(); }
            else if (part != ".") segments.push_back(part);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        std::string result = origin;
        for (const auto &part : segments) result += "/" + part;
        if (segments.empty()) result += "/";
        return result + suffix;
    }

    class LoginSequence {
    public:
        LoginSequence(BedrockConnection &connection, const ClientConnectionSettings &settings,
                      const KeyPair &key, std::string authJson, std::string clientJwt)
                : mConnection(connection), mSettings(settings), mKey(key), mAuthJson(std::move(authJson)),
                  mClientJwt(std::move(clientJwt)), mDone(false), mWaitingForSpawn(false),
                  mGameDataReceived(false), mChunkRadius(0) {
        }

        bool run(std::string &outError) {
            _expect({MinecraftPacketIds::NetworkSettings, MinecraftPacketIds::PlayStatus});

            RequestNetworkSettingsPacket request;
            request.mProtocolVersion = mSettings.mProtocolVersion;
            mConnection.send(request);
            mConnection.flush();

            mLastActivity = std::chrono::steady_clock::now();
            std::string payload;

            while (!mDone) {
                if (mSettings.mCancel != nullptr && mSettings.mCancel->load()) {
                    outError = "dial cancelled";
                    return false;
                }

                if (mAwaitingDecision) {
                    mLastActivity = std::chrono::steady_clock::now();
                    if (!_pollDecision(outError)) return false;
                }

                if (mRelayingPacks && mSettings.mResourcePacks.mRelayTick) {
                    bool activity = false;
                    bool completed = false;
                    if (!mSettings.mResourcePacks.mRelayTick(mConnection, activity, completed, outError))
                        return false;
                    if (activity)
                        mLastActivity = std::chrono::steady_clock::now();
                    if (completed) {
                        mRelayingPacks = false;
                        _expect({MinecraftPacketIds::DimensionData, MinecraftPacketIds::StartGame});
                        if (mSettings.mDeferGameData) {
                            mDone = true;
                            mConnection.flush();
                            break;
                        }
                    }
                }

                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - mLastActivity).count();

                if (elapsed >= (long long) (mRelayingPacks ? mSettings.mResourcePacks.mRelayTimeoutMs : mSettings.mTimeoutMs)) {
                    outError = "dial timed out after " + std::to_string(elapsed) + " ms; waiting for " + _expectedPackets();
                    _trace(outError + "; pending chunk packs=" + std::to_string(mDownloads.size()));
                    for (const auto &download : mDownloads)
                        _trace("pack " + download.mOffer.mPackId + " chunks=" + std::to_string(download.mReceivedChunks) + "/" + std::to_string(download.mChunkCount));
                    return false;
                }

                mConnection.update();

                bool received = false;

                while (!mDone && _receivePending(payload)) {
                    received = true;
                    mLastActivity = std::chrono::steady_clock::now();

                    if (!_receive(std::move(payload), outError))
                        return false;
                }

                if (mConnection.isClosed()) {
                    outError = "connection closed: " + mConnection.getDisconnectReason();
                    _trace(outError + "; waiting for " + _expectedPackets());
                    return false;
                }

                if (!received)
                    std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_WAIT_MS));
            }

            mConnection.setGameData(mStartGame, mItemRegistry, mChunkRadius);

            for (std::string &deferred: mDeferred)
                mConnection.deferRaw(std::move(deferred));

            mDeferred.clear();
            return true;
        }

        std::vector<DownloadedResourcePack> takeResourcePacks() {
            return std::move(mCompletedPacks);
        }

        std::vector<ResourcePackOffer> takeOfferedPacks() {
            return std::move(mOfferedPacks);
        }

    private:
        bool _receivePending(std::string &payload) {
            if (mCdnPackets.empty()) return mConnection.receiveRaw(payload);
            payload = std::move(mCdnPackets.front());
            mCdnPacketBytes -= payload.size();
            mCdnPackets.pop_front();
            return true;
        }

        void _trace(const std::string &message) const {
            if (mSettings.mDiagnostic) mSettings.mDiagnostic(message);
        }

        std::string _expectedPackets() const {
            std::string result;
            for (auto id : mExpected) {
                if (!result.empty()) result += ',';
                result += std::to_string(static_cast<int>(id));
            }
            return result.empty() ? "pack decision" : "packet IDs [" + result + "]";
        }

        struct PackDownload {
            ResourcePackOffer mOffer;
            std::string mData;
            int64_t mChunkSize = 0;
            int64_t mChunkCount = 0;
            int64_t mReceivedChunks = 0;
            bool mStarted = false;
            std::vector<bool> mReceived;
            std::string mHash;
            std::string mWireVersion;
        };

        static std::string _packKey(const std::string &id, const std::string &version) {
            return id + "_" + version;
        }

        void _reportProgress() {
            if (!mSettings.mResourcePacks.mProgress)
                return;

            uint64_t received = mCdnReceived.load();
            uint64_t total = std::max(mCdnPendingBytes, received);

            for (const PackDownload &download: mDownloads) {
                total += download.mStarted ? (uint64_t) download.mData.size() : download.mOffer.mPackSize;
                received += download.mStarted && download.mChunkCount > 0
                            ? (uint64_t) download.mData.size() * (uint64_t) download.mReceivedChunks / (uint64_t) download.mChunkCount
                            : 0;
            }

            for (const DownloadedResourcePack &pack: mCompletedPacks) {
                total += pack.mData.size();
                received += pack.mData.size();
            }

            mSettings.mResourcePacks.mProgress(received, total);
            auto now = std::chrono::steady_clock::now();
            if (now - mLastProgressLog >= std::chrono::seconds(2)) {
                mLastProgressLog = now;
                _trace("packs progress bytes=" + std::to_string(received) + "/" + std::to_string(total)
                       + " chunk-packs=" + std::to_string(mDownloads.size()) + " completed=" + std::to_string(mCompletedPacks.size()));
            }
        }

        void _finishPacks() {
            _trace("packs sending HaveAllPacks");
            ResourcePackClientResponsePacket response;
            response.mStatus = ResourcePackClientResponsePacket::Status::HaveAllPacks;
            mConnection.send(response);
            mConnection.flush();
            _expect({MinecraftPacketIds::ResourcePackStack});
        }

        bool _pollDecision(std::string &outError) {
            ResourcePackDecision decision = mSettings.mResourcePacks.mDecision
                                            ? mSettings.mResourcePacks.mDecision()
                                            : ResourcePackDecision::Skip;
            if (decision == ResourcePackDecision::Pending)
                return true;

            mAwaitingDecision = false;
            _trace(decision == ResourcePackDecision::Skip ? "packs decision=skip" : "packs decision=download");

            if (decision == ResourcePackDecision::Skip) {
                for (const auto &download : mDownloads)
                    if (download.mOffer.mRequired) { outError = "required resource packs were declined"; return false; }
                for (const auto &offer : mCdnDownloads)
                    if (offer.mRequired) { outError = "required resource packs were declined"; return false; }
                mOfferedPacks.erase(std::remove_if(mOfferedPacks.begin(), mOfferedPacks.end(), [&](const ResourcePackOffer &offer) {
                    for (const auto &download : mDownloads)
                        if (download.mOffer.mPackId == offer.mPackId && download.mOffer.mPackVersion == offer.mPackVersion) return true;
                    for (const auto &pending : mCdnDownloads)
                        if (pending.mPackId == offer.mPackId && pending.mPackVersion == offer.mPackVersion) return true;
                    return false;
                }), mOfferedPacks.end());
                mSkippedPacks = true;
                mDownloads.clear();
                mCdnDownloads.clear();
                _finishPacks();
                return true;
            }

            if (!_downloadCdnPacks(outError)) return false;
            mLastActivity = std::chrono::steady_clock::now();
            if (mSettings.mCancel && mSettings.mCancel->load()) { outError = "pack download cancelled"; return false; }

            ResourcePackClientResponsePacket request;
            request.mStatus = ResourcePackClientResponsePacket::Status::SendPacks;

            for (const PackDownload &download: mDownloads)
                request.mPackIds.push_back(_packKey(download.mOffer.mPackId, download.mOffer.mPackVersion));

            if (request.mPackIds.empty()) {
                _finishPacks();
                return true;
            }

            _expect({MinecraftPacketIds::ResourcePackDataInfo, MinecraftPacketIds::ResourcePackChunkData,
                     MinecraftPacketIds::ResourcePackStack});
            mConnection.send(request);
            mConnection.flush();
            _reportProgress();
            return true;
        }

        /**
         * Fetches the packs the server hosts on a CDN over HTTP, following
         * redirects, the way the game does instead of asking for chunks.
         */
        bool _downloadCdnPacks(std::string &outError) {
            std::vector<ResourcePackOffer> pending = std::move(mCdnDownloads);
            mCdnDownloads.clear();
            mCdnPendingBytes = 0;
            for (const auto &offer : pending) mCdnPendingBytes += offer.mPackSize;
            _reportProgress();

            for (const ResourcePackOffer &offer: pending) {
                _trace("CDN start pack=" + offer.mPackId + " version=" + offer.mPackVersion + " expected-bytes=" + std::to_string(offer.mPackSize));
                if (mSettings.mCancel && mSettings.mCancel->load()) {
                    outError = "pack download cancelled";
                    return false;
                }

                std::atomic<bool> cancelDownload {false};
                auto task = std::async(std::launch::async, [&, offer] {
                    std::string url = offer.mCdnUrl;
                    HttpResponse response;
                    std::string error;
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(CDN_TIMEOUT_MS);
                    for (int hop = 0; hop <= 5; ++hop) {
                        int remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count());
                        if (remaining <= 0 || cancelDownload.load()) break;
                        if (!HttpClient::get(url, {{"Accept-Encoding", "identity"}}, response, error, remaining,
                                static_cast<size_t>(std::min<uint64_t>(offer.mPackSize ? offer.mPackSize : MAX_PACK_SIZE, MAX_STACK_SIZE - mAcquiredPackBytes) + 1024 * 1024), &cancelDownload,
                                [&](size_t bytes) { mCdnReceived = offer.mPackSize ? std::min<uint64_t>(bytes, offer.mPackSize) : bytes; })) {
                            _trace("CDN request failed pack=" + offer.mPackId + " hop=" + std::to_string(hop) + " reason="
                                   + (error.rfind("unsupported URL", 0) == 0 ? std::string("unsupported URL scheme or format") : error));
                            break;
                        }
                        _trace("CDN response pack=" + offer.mPackId + " hop=" + std::to_string(hop) + " status=" + std::to_string(response.mStatus)
                               + " bytes=" + std::to_string(response.mBody.size()) + " expected=" + std::to_string(offer.mPackSize));
                        if (response.mStatus == 301 || response.mStatus == 302 || response.mStatus == 303 || response.mStatus == 307 || response.mStatus == 308) {
                            url = redirectUrl(url, response.getHeader("Location"));
                            if (url.empty()) break;
                            continue;
                        }
                        if (response.mStatus == 200 && !response.mBody.empty() && response.mBody.size() <= MAX_PACK_SIZE
                                && (!offer.mPackSize || response.mBody.size() == offer.mPackSize)) return std::move(response.mBody);
                        break;
                    }
                    return std::string();
                });
                while (task.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
                    mConnection.update();
                    mConnection.flush();
                    std::string payload;
                    while (mConnection.receiveRaw(payload)) {
                        if (payload.size() > MAX_CDN_PACKET_BYTES - mCdnPacketBytes) {
                            mConnection.close("pending packets exceeded CDN memory budget");
                            cancelDownload = true;
                            break;
                        }
                        mCdnPacketBytes += payload.size();
                        mCdnPackets.push_back(std::move(payload));
                    }
                    if (mConnection.isClosed() || (mSettings.mCancel && mSettings.mCancel->load()))
                        cancelDownload = true;
                    _reportProgress();
                }
                DownloadedResourcePack pack {offer, task.get()};
                if (mConnection.isClosed()) {
                    outError = "connection closed during CDN download: " + mConnection.getDisconnectReason();
                    _trace(outError);
                    return false;
                }
                if (mSettings.mCancel && mSettings.mCancel->load()) {
                    outError = "pack download cancelled";
                    return false;
                }
                mCdnPendingBytes -= offer.mPackSize;
                mCdnReceived = 0;
                std::string error;
                if (pack.mData.size() > MAX_STACK_SIZE - mAcquiredPackBytes) { outError = "resource pack stack exceeds memory budget"; return false; }
                if (pack.mData.empty() || (mSettings.mResourcePacks.mValidate && !mSettings.mResourcePacks.mValidate(pack, error))) {
                    _trace("CDN fallback to server chunks pack=" + offer.mPackId + " reason=" + (error.empty() ? "HTTP download failed or size mismatch" : error));
                    LOG_WARN(LogAreaID::Network, "CDN pack %s unavailable or invalid; requesting server chunks", offer.mPackId.c_str());
                    mDownloads.push_back({offer});
                } else {
                    _trace("CDN validated pack=" + offer.mPackId);
                    mAcquiredPackBytes += pack.mData.size();
                    mCompletedPacks.push_back(std::move(pack));
                }
                _reportProgress();
            }
            return true;
        }

        PackDownload *_findDownload(const Uuid &id, const std::string &version) {
            const std::string text = id.toString();
            PackDownload *match = nullptr;
            for (PackDownload &download: mDownloads) {
                if (download.mOffer.mPackId != text) continue;
                if (!version.empty()) {
                    if (download.mOffer.mPackVersion == version) return &download;
                    continue;
                }
                // Some servers identify transfers by UUID alone. Never guess between versions.
                if (match != nullptr) return nullptr;
                match = &download;
            }
            return match;
        }

        bool _handleResourcePackDataInfo(std::string payload, std::string &outError) {
            std::shared_ptr<ResourcePackDataInfoPacket> packet =
                    _decode<ResourcePackDataInfoPacket>(std::move(payload), outError);
            if (packet == nullptr)
                return false;

            PackDownload *download = _findDownload(packet->mPackId, packet->mPackVersion);
            _trace("pack data-info received id=" + packet->mPackId.toString() + " wire-version="
                   + (packet->mPackVersion.empty() ? "<absent>" : packet->mPackVersion));
            if (download == nullptr) {
                outError = "unknown or ambiguous pack data-info: " + packet->mPackId.toString() + " version=" + packet->mPackVersion;
                _trace(outError);
                return false;
            }
            if (download->mStarted) return true;
            download->mWireVersion = packet->mPackVersion;
            _trace("pack transfer matched id=" + download->mOffer.mPackId + " offered-version=" + download->mOffer.mPackVersion);
            const int64_t expectedChunks = packet->mMaxChunkSize > 0 && packet->mCompressedPackSize > 0
                    ? (packet->mCompressedPackSize - 1) / packet->mMaxChunkSize + 1 : -1;
            _trace("pack metadata values id=" + packet->mPackId.toString()
                   + " count=" + std::to_string(packet->mChunkCount)
                   + " expected-count=" + std::to_string(expectedChunks)
                   + " max-chunk-bytes=" + std::to_string(packet->mMaxChunkSize)
                   + " compressed-bytes=" + std::to_string(packet->mCompressedPackSize)
                   + " offered-bytes=" + std::to_string(download->mOffer.mPackSize)
                   + " hash-bytes=" + std::to_string(packet->mHash.size())
                   + " type=" + std::to_string(static_cast<int>(packet->mType))
                   + " premium=" + (packet->mPremium ? "yes" : "no"));
            std::string invalid;
            auto reject = [&](bool condition, const char *reason) {
                if (!condition) return;
                if (!invalid.empty()) invalid += "; ";
                invalid += reason;
            };
            reject(packet->mMaxChunkSize <= 0, "non-positive maximum chunk size");
            reject(packet->mCompressedPackSize <= 0, "non-positive compressed size");
            reject(packet->mCompressedPackSize > 0 && uint64_t(packet->mCompressedPackSize) > MAX_PACK_SIZE, "compressed size exceeds 512 MiB");
            reject(expectedChunks > 1048576, "calculated chunk count exceeds limit");
            reject(!packet->mHash.empty() && packet->mHash.size() != SHA256_DIGEST_LENGTH, "hash length is not 32 bytes");
            if (!invalid.empty()) {
                outError = "invalid resource pack chunk metadata: " + invalid;
                _trace("pack metadata rejected id=" + packet->mPackId.toString() + ": " + invalid);
                return false;
            }

            // Some servers report the last chunk index instead of the count.
            // Derive the transfer length from the validated byte sizes.
            if (packet->mChunkCount != expectedChunks)
                _trace("pack chunk count corrected id=" + download->mOffer.mPackId + " announced="
                       + std::to_string(packet->mChunkCount) + " calculated=" + std::to_string(expectedChunks));
            download->mReceived.assign(static_cast<size_t>(expectedChunks), false);
            download->mHash = packet->mHash;
            _trace("pack chunk metadata id=" + download->mOffer.mPackId + " count=" + std::to_string(expectedChunks)
                   + " chunk-size=" + std::to_string(packet->mMaxChunkSize) + " total=" + std::to_string(packet->mCompressedPackSize));
            download->mStarted = true;
            download->mChunkSize = packet->mMaxChunkSize;
            download->mChunkCount = expectedChunks;
            const uint64_t archiveBytes = static_cast<uint64_t>(packet->mCompressedPackSize);
            if (archiveBytes > MAX_STACK_SIZE - mAcquiredPackBytes) { outError = "resource pack stack exceeds memory budget"; return false; }
            mAcquiredPackBytes += archiveBytes;
            download->mData.assign(static_cast<size_t>(archiveBytes), '\0');

            for (int64_t chunk = 0; chunk < std::min<int64_t>(download->mChunkCount, 16); ++chunk) {
                ResourcePackChunkRequestPacket request;
                request.mPackId = packet->mPackId;
                request.mPackVersion = download->mWireVersion;
                request.mChunkIndex = (int32_t) chunk;
                mConnection.send(request);
            }

            mConnection.flush();
            _reportProgress();
            return true;
        }

        bool _handleResourcePackChunkData(std::string payload, std::string &outError) {
            std::shared_ptr<ResourcePackChunkDataPacket> packet =
                    _decode<ResourcePackChunkDataPacket>(std::move(payload), outError);
            if (packet == nullptr)
                return false;

            PackDownload *download = _findDownload(packet->mPackId, packet->mPackVersion);
            if (download == nullptr) {
                _trace("unmatched pack chunk id=" + packet->mPackId.toString() + " wire-version=" + packet->mPackVersion
                       + " index=" + std::to_string(packet->mChunkIndex));
                return true;
            }
            if (!download->mStarted || packet->mChunkIndex < 0) {
                outError = "resource pack chunk received before metadata or with invalid index";
                _trace(outError);
                return false;
            }

            if (packet->mChunkIndex >= download->mChunkCount) { outError = "invalid resource pack chunk index"; return false; }
            const size_t index = static_cast<size_t>(packet->mChunkIndex);
            const size_t offset = index * static_cast<size_t>(download->mChunkSize);
            const size_t expected = std::min(static_cast<size_t>(download->mChunkSize), download->mData.size() - offset);
            if (packet->mData.size() != expected) { outError = "truncated resource pack chunk"; return false; }
            if (download->mReceived[index]) return true;
            download->mData.replace(offset, expected, packet->mData);
            download->mReceived[index] = true;
            ++download->mReceivedChunks;
            if (download->mReceivedChunks == 1 || download->mReceivedChunks % 64 == 0 || download->mReceivedChunks == download->mChunkCount)
                _trace("pack chunks id=" + download->mOffer.mPackId + " received=" + std::to_string(download->mReceivedChunks) + "/" + std::to_string(download->mChunkCount));
            if (packet->mChunkIndex + 16 < download->mChunkCount) {
                ResourcePackChunkRequestPacket request;
                request.mPackId = packet->mPackId;
                request.mPackVersion = download->mWireVersion;
                request.mChunkIndex = packet->mChunkIndex + 16;
                mConnection.send(request);
            }

            if (download->mReceivedChunks >= download->mChunkCount) {
                if (!download->mHash.empty()) {
                    unsigned char hash[SHA256_DIGEST_LENGTH];
                    SHA256(reinterpret_cast<const unsigned char *>(download->mData.data()), download->mData.size(), hash);
                    if (download->mHash != std::string(reinterpret_cast<const char *>(hash), sizeof(hash))) {
                        outError = "resource pack SHA-256 mismatch";
                        return false;
                    }
                }
                DownloadedResourcePack pack {download->mOffer, std::move(download->mData)};
                if (mSettings.mResourcePacks.mValidate && !mSettings.mResourcePacks.mValidate(pack, outError)) return false;
                mCompletedPacks.push_back(std::move(pack));
                const std::string id = download->mOffer.mPackId;
                const std::string version = download->mOffer.mPackVersion;
                mDownloads.erase(std::remove_if(mDownloads.begin(), mDownloads.end(), [&](const PackDownload &entry) {
                    return entry.mOffer.mPackId == id && entry.mOffer.mPackVersion == version;
                }), mDownloads.end());
            }

            _reportProgress();

            if (mDownloads.empty())
                _finishPacks();

            return true;
        }

        void _expect(std::initializer_list<MinecraftPacketIds> ids) {
            mExpected.assign(ids.begin(), ids.end());
            _trace("login waiting for " + _expectedPackets());
        }

        bool _isExpected(MinecraftPacketIds id) const {
            for (MinecraftPacketIds expected: mExpected) {
                if (expected == id)
                    return true;
            }

            return false;
        }

        template<class PacketType>
        std::shared_ptr<PacketType> _decode(std::string payload, std::string &outError) const {
            std::shared_ptr<Packet> packet = mConnection.decode(std::move(payload));
            std::shared_ptr<PacketType> typed = std::dynamic_pointer_cast<PacketType>(packet);

            if (typed == nullptr)
                outError = std::string("could not decode ") + PacketType().getName() + ": "
                           + mConnection.getLastDecodeError();

            return typed;
        }

        bool _receive(std::string payload, std::string &outError) {
            MinecraftPacketIds id;
            if (!BedrockConnection::peekPacketId(payload, id))
                return true;

            if (mSettings.mPacketObserver)
                mSettings.mPacketObserver(id);

            if (id == MinecraftPacketIds::PacketViolationWarning) {
                const auto warning = _decode<PacketViolationWarningPacket>(std::move(payload), outError);
                if (!warning) return false;
                const std::string reason = "server rejected packet " + std::to_string(warning->mPacketCauseId)
                    + ": " + warning->mContext;
                _trace(reason);
                if (warning->mSeverity == PacketViolationSeverity::TerminatingConnection) {
                    outError = reason;
                    return false;
                }
                return true;
            }

            if (!_isExpected(id)) {
                mDeferred.push_back(std::move(payload));
                if (id == MinecraftPacketIds::Transfer && mStartGame != nullptr) {
                    _trace("login Transfer before ItemRegistry; finishing so the client can follow it");
                    mDone = true;
                }
                return true;
            }

            bool handled = false;

            if (mSettings.mResourcePacks.mRelayPacket &&
                (id == MinecraftPacketIds::ResourcePacksInfo || id == MinecraftPacketIds::ResourcePackStack ||
                 id == MinecraftPacketIds::ResourcePackDataInfo || id == MinecraftPacketIds::ResourcePackChunkData)) {
                if (!mSettings.mResourcePacks.mRelayTick) {
                    outError = "resource pack relay requires a tick handler";
                    return false;
                }
                mRelayingPacks = true;
                _expect({MinecraftPacketIds::ResourcePacksInfo, MinecraftPacketIds::ResourcePackStack,
                         MinecraftPacketIds::ResourcePackDataInfo, MinecraftPacketIds::ResourcePackChunkData});
                return mSettings.mResourcePacks.mRelayPacket(id, payload, mConnection, outError);
            }

            switch (id) {
                case MinecraftPacketIds::NetworkSettings:
                    handled = _handleNetworkSettings(std::move(payload), outError);
                    break;

                case MinecraftPacketIds::ServerToClientHandshake:
                    handled = _handleServerToClientHandshake(std::move(payload), outError);
                    break;

                case MinecraftPacketIds::PlayStatus:
                    handled = _handlePlayStatus(std::move(payload), outError);
                    break;

                case MinecraftPacketIds::ResourcePacksInfo:
                    handled = _handleResourcePacksInfo(std::move(payload), outError);
                    break;

                case MinecraftPacketIds::ResourcePackStack:
                    handled = _handleResourcePackStack(std::move(payload), outError);
                    break;

                case MinecraftPacketIds::ResourcePackDataInfo:
                    handled = _handleResourcePackDataInfo(std::move(payload), outError);
                    break;

                case MinecraftPacketIds::ResourcePackChunkData:
                    handled = _handleResourcePackChunkData(std::move(payload), outError);
                    break;

                case MinecraftPacketIds::DimensionData:
                    mDeferred.push_back(std::move(payload));
                    handled = true;
                    break;

                case MinecraftPacketIds::StartGame:
                    handled = _handleStartGame(std::move(payload), outError);
                    break;

                case MinecraftPacketIds::ItemRegistry:
                    handled = _handleItemRegistry(std::move(payload), outError);
                    break;

                case MinecraftPacketIds::ChunkRadiusUpdated:
                    handled = _handleChunkRadiusUpdated(std::move(payload), outError);
                    break;

                default:
                    handled = true;
                    break;
            }

            mConnection.flush();
            return handled;
        }

        bool _handleNetworkSettings(std::string payload, std::string &outError) {
            std::shared_ptr<NetworkSettingsPacket> packet = _decode<NetworkSettingsPacket>(std::move(payload),
                                                                                           outError);
            if (packet == nullptr)
                return false;

            const int algorithm = (int) packet->mCompressionAlgorithm;
            _trace("compression algorithm=" + std::to_string(algorithm) + " threshold=" + std::to_string(packet->mCompressionThreshold));

            if (algorithm == (int) CompressedNetworkPeer::CompressionAlgorithm::ZLib) {
                mConnection.enableCompression(CompressedNetworkPeer::CompressionAlgorithm::ZLib,
                                              packet->mCompressionThreshold);
            } else if (algorithm == (int) CompressedNetworkPeer::CompressionAlgorithm::Snappy) {
                mConnection.enableCompression(CompressedNetworkPeer::CompressionAlgorithm::Snappy,
                                              packet->mCompressionThreshold);
            } else if (algorithm == COMPRESSION_NONE_ID
                       || algorithm == (int) CompressedNetworkPeer::CompressionAlgorithm::None) {
                mConnection.enableCompression(CompressedNetworkPeer::CompressionAlgorithm::None,
                                              packet->mCompressionThreshold);
            } else {
                outError = "unsupported compression algorithm " + std::to_string(algorithm);
                return false;
            }

            _expect({MinecraftPacketIds::ServerToClientHandshake, MinecraftPacketIds::PlayStatus});

            LoginPacket login;
            login.mProtocolVersion = mSettings.mProtocolVersion;
            login.mAuthJwt = std::move(mAuthJson);
            login.mClientJwt = std::move(mClientJwt);
            mConnection.send(login);
            mConnection.flush();
            return true;
        }

        bool _handleServerToClientHandshake(std::string payload, std::string &outError) {
            std::shared_ptr<ServerToClientHandshakePacket> packet =
                    _decode<ServerToClientHandshakePacket>(std::move(payload), outError);
            if (packet == nullptr)
                return false;

            EncryptionKey key{};
            if (!EncryptionHandshake::acceptServerToken(packet->mJwt, mKey, key)) {
                outError = "could not verify the server handshake token";
                return false;
            }

            if (!mConnection.enableEncryption(key)) {
                outError = "could not enable encryption";
                return false;
            }

            ClientToServerHandshakePacket response;
            mConnection.send(response);
            return true;
        }

        bool _handlePlayStatus(std::string payload, std::string &outError) {
            std::shared_ptr<PlayStatusPacket> packet = _decode<PlayStatusPacket>(std::move(payload), outError);
            if (packet == nullptr)
                return false;

            switch (packet->mStatus) {
                case PlayStatusPacket::Status::LoginSuccess: {
                    if (mLoginSuccessReceived) {
                        _trace("login LoginSuccess repeated; ignored so the packs already agreed on stay");
                        return true;
                    }
                    mLoginSuccessReceived = true;
                    ClientCacheStatusPacket cacheStatus;
                    cacheStatus.mSupported = mSettings.mEnableClientCache;
                    mConnection.send(cacheStatus);

                    _expect({MinecraftPacketIds::ResourcePacksInfo});
                    return true;
                }

                case PlayStatusPacket::Status::PlayerSpawn:
                    mWaitingForSpawn = true;
                    _tryFinalise();
                    return true;

                case PlayStatusPacket::Status::LoginFailedClientOld:
                    outError = "client outdated";
                    break;

                case PlayStatusPacket::Status::LoginFailedServerOld:
                    outError = "server outdated";
                    break;

                case PlayStatusPacket::Status::LoginFailedInvalidTenant:
                    outError = "invalid edu edition game owner";
                    break;

                case PlayStatusPacket::Status::LoginFailedEditionMismatchEduToVanilla:
                    outError = "cannot join an edu edition game on vanilla";
                    break;

                case PlayStatusPacket::Status::LoginFailedEditionMismatchVanillaToEdu:
                    outError = "cannot join a vanilla game on edu edition";
                    break;

                case PlayStatusPacket::Status::FailedServerFullSubClient:
                    outError = "server full";
                    break;

                case PlayStatusPacket::Status::EditorToVanillaMismatch:
                    outError = "cannot join a vanilla game on editor";
                    break;

                case PlayStatusPacket::Status::VanillaToEditorMismatch:
                    outError = "cannot join an editor game on vanilla";
                    break;

                default:
                    outError = "unknown play status " + std::to_string((int) packet->mStatus);
                    break;
            }

            mConnection.close(outError);
            return false;
        }

        bool _handleResourcePacksInfo(std::string payload, std::string &outError) {
            std::shared_ptr<ResourcePacksInfoPacket> packet = _decode<ResourcePacksInfoPacket>(std::move(payload),
                                                                                               outError);
            if (packet == nullptr)
                return false;

            std::vector<ResourcePackOffer> offers;
            mOfferedPacks.clear();
            mSkippedPacks = false;
            mDownloads.clear();
            mCdnDownloads.clear();
            mCompletedPacks.clear();
            mAcquiredPackBytes = 0;

            if (packet->mResourcePackInfos.size() > MAX_PACK_COUNT) { outError = "resource pack count exceeds limit"; return false; }
            uint64_t offeredBytes = 0;
            for (const ResourcePacksInfoPacket::Entry &entry: packet->mResourcePackInfos) {
                ResourcePackOffer offer;
                offer.mPackId = entry.mPackId.toString();
                offer.mPackVersion = entry.mPackVersion;
                offer.mPackSize = entry.mPackSize;
                offer.mContentKey = entry.mContentKey;
                offer.mSubPackName = entry.mSubPackName;
                offer.mCdnUrl = entry.mCdnUrl;
                offer.mRequired = packet->mForcedToAccept;
                _trace("pack offered id=" + offer.mPackId + " version=" + offer.mPackVersion + " bytes=" + std::to_string(offer.mPackSize)
                       + " cdn=" + (offer.mCdnUrl.empty() ? "no" : "yes") + " encrypted=" + (offer.mContentKey.empty() ? "no" : "yes"));
                if (offer.mPackSize > MAX_PACK_SIZE) { outError = "resource pack exceeds 512 MiB limit"; return false; }
                const uint64_t reservedBytes = offer.mPackSize;
                if (reservedBytes > MAX_STACK_SIZE - offeredBytes) { outError = "resource pack stack exceeds memory budget"; return false; }
                offeredBytes += reservedBytes;
                mOfferedPacks.push_back(offer);

                if (mSettings.mResourcePacks.mIsCached && mSettings.mResourcePacks.mIsCached(offer)) {
                    _trace("pack cache hit id=" + offer.mPackId);
                    continue;
                }
                _trace("pack cache miss id=" + offer.mPackId);

                offers.push_back(offer);

                if (offer.mCdnUrl.empty())
                    mDownloads.push_back({offer});
                else
                    mCdnDownloads.push_back(offer);
            }

            if (offers.empty() || !mSettings.mResourcePacks.mOffer) {
                if (!offers.empty()) {
                    for (const auto &offer : offers) {
                        if (offer.mRequired) { outError = "required resource packs have no download handler"; return false; }
                    }
                    mSkippedPacks = true;
                }
                mDownloads.clear();
                mCdnDownloads.clear();
                _finishPacks();
                return true;
            }

            _expect({});
            mSettings.mResourcePacks.mOffer(offers);
            mAwaitingDecision = true;
            return true;
        }

        bool _handleResourcePackStack(std::string payload, std::string &outError) {
            std::shared_ptr<ResourcePackStackPacket> packet = _decode<ResourcePackStackPacket>(std::move(payload),
                                                                                               outError);
            if (packet == nullptr)
                return false;

            if (!mSkippedPacks && (!mDownloads.empty() || !mCdnDownloads.empty())) {
                outError = "server completed resource packs before all downloads finished";
                return false;
            }
            std::vector<ResourcePackOffer> active;
            {
                for (const auto &entry : packet->mResourcePacks) {
                    auto offer = std::find_if(mOfferedPacks.begin(), mOfferedPacks.end(), [&](const ResourcePackOffer &candidate) {
                        return candidate.mPackId == entry.mPackId && candidate.mPackVersion == entry.mPackVersion;
                    });
                    if (offer == mOfferedPacks.end()) {
                        _trace("resource pack stack names unoffered pack " + entry.mPackId + " " + entry.mPackVersion);
                        continue;
                    }
                    active.push_back(*offer);
                    active.back().mSubPackName = entry.mSubPackName;
                }
            }
            mOfferedPacks = std::move(active);
            _expect({MinecraftPacketIds::DimensionData, MinecraftPacketIds::StartGame});

            ResourcePackClientResponsePacket response;
            response.mStatus = ResourcePackClientResponsePacket::Status::Completed;
            mConnection.send(response);
            return true;
        }

        bool _handleStartGame(std::string payload, std::string &outError) {
            mStartGame = _decode<StartGamePacket>(std::move(payload), outError);
            if (mStartGame == nullptr)
                return false;

            ServerboundLoadingScreenPacket loading;
            loading.mType = ServerboundLoadingScreenPacket::Type::StartLoadingScreen;
            mConnection.send(loading);

            _expect({MinecraftPacketIds::ItemRegistry});
            return true;
        }

        bool _handleItemRegistry(std::string payload, std::string &outError) {
            mItemRegistry = _decode<ItemRegistryPacket>(std::move(payload), outError);
            if (mItemRegistry == nullptr)
                return false;

            RequestChunkRadiusPacket request;
            request.mRadius = mSettings.mChunkRadius;
            request.mMaxRadius = mSettings.mChunkRadius;
            mConnection.send(request);

            _expect({MinecraftPacketIds::ChunkRadiusUpdated, MinecraftPacketIds::PlayStatus});
            return true;
        }

        bool _handleChunkRadiusUpdated(std::string payload, std::string &outError) {
            std::shared_ptr<ChunkRadiusUpdatedPacket> packet =
                    _decode<ChunkRadiusUpdatedPacket>(std::move(payload), outError);
            if (packet == nullptr)
                return false;

            if (packet->mRadius < 1) {
                outError = "expected chunk radius of at least 1, got " + std::to_string(packet->mRadius);
                return false;
            }

            _expect({MinecraftPacketIds::PlayStatus});

            mChunkRadius = packet->mRadius;
            mGameDataReceived = true;
            _tryFinalise();
            return true;
        }

        void _tryFinalise() {
            if (mSettings.mDeferSpawn) {
                if (!mGameDataReceived)
                    return;

                mConnection.setSpawnReceived(mWaitingForSpawn);
                mDone = true;
                return;
            }

            if (!mWaitingForSpawn || !mGameDataReceived)
                return;

            mConnection.setSpawnReceived(true);
            mWaitingForSpawn = false;
            mGameDataReceived = false;

            ServerboundLoadingScreenPacket loading;
            loading.mType = ServerboundLoadingScreenPacket::Type::EndLoadingScreen;
            mConnection.send(loading);

            SetLocalPlayerAsInitializedPacket initialized;
            initialized.mRuntimeActorId = mStartGame != nullptr ? mStartGame->mRuntimeActorId : 0;
            mConnection.send(initialized);
            mConnection.flush();

            mDone = true;
        }

        BedrockConnection &mConnection;
        const ClientConnectionSettings &mSettings;
        const KeyPair &mKey;
        std::string mAuthJson;
        std::string mClientJwt;
        std::vector<MinecraftPacketIds> mExpected;
        std::vector<std::string> mDeferred;
        std::deque<std::string> mCdnPackets;
        size_t mCdnPacketBytes = 0;
        uint64_t mAcquiredPackBytes = 0;
        std::shared_ptr<StartGamePacket> mStartGame;
        std::shared_ptr<ItemRegistryPacket> mItemRegistry;
        bool mDone;
        bool mWaitingForSpawn;
        bool mGameDataReceived;
        int mChunkRadius;
        bool mAwaitingDecision = false;
        bool mLoginSuccessReceived = false;
        std::chrono::steady_clock::time_point mLastActivity;
        std::vector<PackDownload> mDownloads;
        bool mSkippedPacks = false;
        std::chrono::steady_clock::time_point mLastProgressLog {};
        uint64_t mCdnPendingBytes = 0;
        std::atomic<uint64_t> mCdnReceived {0};
        std::vector<ResourcePackOffer> mCdnDownloads;
        std::vector<DownloadedResourcePack> mCompletedPacks;
        std::vector<ResourcePackOffer> mOfferedPacks;
        bool mRelayingPacks = false;
    };

    const char *AUTHORIZATION_SERVICE_NAME = "auth";

    std::string hostnameOf(const std::string &url) {
        size_t start = url.find("://");
        start = start == std::string::npos ? 0 : start + 3;

        size_t end = url.find_first_of(":/?#", start);
        if (end == std::string::npos)
            end = url.size();

        return url.substr(start, end - start);
    }

    bool parseNetworkId(const std::string &text, uint64_t &out) {
        if (text.empty() || text.size() > 20)
            return false;

        uint64_t value = 0;

        for (char character: text) {
            if (character < '0' || character > '9')
                return false;

            const uint64_t digit = (uint64_t) (character - '0');

            if (value > (UINT64_MAX - digit) / 10)
                return false;

            value = value * 10 + digit;
        }

        out = value;
        return true;
    }

    bool createSignaling(const ClientConnectionSettings &settings, std::shared_ptr<nethernet::Signaling> &out,
                         std::string &outError) {
        switch (settings.mNetherNet.mSignalingType) {
            case NetherNetSignalingType::Lan: {
                std::shared_ptr<nethernet::DiscoveryDialer> discovery = std::make_shared<nethernet::DiscoveryDialer>();
                if (!discovery->start(0, outError))
                    return false;

                out = discovery;
                return true;
            }

            case NetherNetSignalingType::WebSocket: {
                if (settings.mAuthentication == nullptr) {
                    outError = "NetherNet signaling requires an authenticated account";
                    return false;
                }

                std::shared_ptr<nethernet::WebSocketSignaling> signaling =
                        std::make_shared<nethernet::WebSocketSignaling>();
                if (!signaling->connect(*settings.mAuthentication, std::string(), outError))
                    return false;

                out = signaling;
                return true;
            }

            case NetherNetSignalingType::JsonRpc: {
                if (settings.mAuthentication == nullptr) {
                    outError = "NetherNet signaling requires an authenticated account";
                    return false;
                }

                std::shared_ptr<nethernet::JsonRpcSignaling> signaling =
                        std::make_shared<nethernet::JsonRpcSignaling>();
                if (!signaling->connect(*settings.mAuthentication, std::string(), outError))
                    return false;

                out = signaling;
                return true;
            }
            case NetherNetSignalingType::Http:
                out = std::make_shared<nethernet::HttpSignaling>(settings.mHost, settings.mPort);
                return true;
        }

        outError = "unknown NetherNet signaling type";
        return false;
    }

    bool dialNetherNet(const ClientConnectionSettings &settings, const std::shared_ptr<KeyPair> &key,
                       const std::string &multiplayerToken, std::shared_ptr<NetherNetClient> &out,
                       std::string &outError) {
        const NetherNetTarget &target = settings.mNetherNet;
        const bool http = target.mSignalingType == NetherNetSignalingType::Http;
        // The HTTP endpoint is addressed by host and port, so the server's network ID is optional.
        const std::string remoteId = http && target.mNetworkId.empty()
                                     ? settings.mHost + ":" + std::to_string(settings.mPort) : target.mNetworkId;

        if (remoteId.empty()) {
            outError = "no NetherNet network ID to dial";
            return false;
        }

        std::shared_ptr<nethernet::Signaling> signaling = target.mSignaling;
        const bool owned = signaling == nullptr;

        if (owned && !createSignaling(settings, signaling, outError))
            return false;

        NetherNetDialOptions options;
        options.mAllowIdentitylessServer = target.mAllowIdentitylessServer;
        options.mDisableTrickleIce = target.mDisableTrickleIce;
        options.mCloseSignalingOnClose = owned;

        nethernet::DiscoveryDialer *discovery = dynamic_cast<nethernet::DiscoveryDialer *>(signaling.get());

        if (discovery != nullptr) {
            uint64_t networkId = 0;
            bool found = false;

            if (!parseNetworkId(target.mNetworkId, networkId)) {
                outError = "LAN network ID is not a uint64: " + target.mNetworkId;
            } else if (!discovery->waitForServer(networkId, settings.mTimeoutMs, settings.mCancel)) {
                outError = "no LAN server answered with network ID " + target.mNetworkId;
            } else {
                found = true;
            }

            if (!found) {
                if (owned)
                    signaling->close();

                return false;
            }
        }

        if (settings.mAuthentication != nullptr) {
            std::string token = multiplayerToken;
            std::string authorizationUri;

            if ((token.empty() && !settings.mAuthentication->requestMultiplayerToken(*key, token, outError)) ||
                !settings.mAuthentication->requestServiceUri(AUTHORIZATION_SERVICE_NAME, authorizationUri,
                                                              outError)) {
                if (owned)
                    signaling->close();

                return false;
            }

            options.mIdentityKey = key;
            options.mIdentityToken = token;
            options.mIdentityDomain = hostnameOf(authorizationUri);
        } else if (!multiplayerToken.empty()) {
            options.mIdentityKey = key;
            options.mIdentityToken = multiplayerToken;
            options.mIdentityDomain = "self";
        }

        std::shared_ptr<NetherNetClient> transport = std::make_shared<NetherNetClient>();

        if (!transport->connect(remoteId, signaling, options, settings.mTimeoutMs, settings.mCancel,
                                outError)) {
            if (owned)
                signaling->close();

            return false;
        }

        out = transport;
        return true;
    }

}

ClientConnectionResult ClientNetworkSystem::dial(const ClientConnectionSettings &settings) {
    ClientConnectionResult result;
    result.mIdentity = settings.mIdentity;
    result.mClientData = settings.mClientData;

    std::shared_ptr<KeyPair> key = KeyPair::generate();
    if (key == nullptr) {
        result.mError = "generating ECDSA key failed";
        return result;
    }

    const bool online = settings.mAuthentication != nullptr;
    MinecraftAuthenticationResult authentication;

    if (online) {
        if (!settings.mAuthentication->authenticate(*key, settings.mLegacyAuthentication, authentication,
                                                    result.mError))
            return result;

        result.mIdentity.mDisplayName = authentication.mDisplayName;
        result.mIdentity.mIdentity = authentication.mIdentity;
        result.mIdentity.mXuid = authentication.mXuid;
        result.mIdentity.mTitleId = authentication.mTitleId;
        if (!authentication.mPlayFabId.empty()) {
            result.mIdentity.mPlayFabId = authentication.mPlayFabId;
            result.mClientData.mPlayFabId = authentication.mPlayFabId;
        }
    }

    const bool netherNetById = settings.mTransportLayer == TransportLayer::NetherNet
                               && settings.mNetherNet.mSignalingType != NetherNetSignalingType::Http;
    const std::string serverAddress = netherNetById
        ? settings.mNetherNet.mNetworkId : settings.mHost + ":" + std::to_string(settings.mPort);
    ClientConnectionRequest::applyIdentityDefaults(result.mIdentity);
    ClientConnectionRequest::applyClientDefaults(result.mClientData, serverAddress, result.mIdentity.mDisplayName,
                                                 settings.mGameVersion);
    std::string authJson;
    std::string clientJwt;
    if (!online) {
        if (!settings.mKeepXboxIdentityData) {
            result.mIdentity.mXuid.clear();
            result.mIdentity.mTitleId.clear();
        }
        if (!ClientConnectionRequest::createOffline(result.mIdentity, result.mClientData, *key,
                                                    settings.mLegacyAuthentication, authJson, clientJwt,
                                                    result.mError))
            return result;
        if (!settings.mLegacyAuthentication) {
            const auto request = json::parse(authJson);
            const auto token = request ? request->get("Token") : nullptr;
            if (token && token->isString()) authentication.mMultiplayerToken = token->mString;
        }
    } else {
        result.mClientData.mGameVersion = settings.mGameVersion;
        if (!ClientConnectionRequest::createOnline(authentication.mChainJson, authentication.mMultiplayerToken,
                                                   result.mClientData, *key, settings.mLegacyAuthentication,
                                                   authJson, clientJwt, result.mError))
            return result;
    }

    std::unique_ptr<BedrockConnection> connection;

    if (settings.mTransportLayer == TransportLayer::NetherNet) {
        std::shared_ptr<NetherNetClient> transport;
        if (!dialNetherNet(settings, key, authentication.mMultiplayerToken, transport, result.mError))
            return result;

        std::shared_ptr<ClientTransport> driver = transport;
        connection.reset(new BedrockConnection(BedrockConnection::Side::Client, transport->getPeer(), driver));
    } else {
        std::shared_ptr<RakNetClient> transport = std::make_shared<RakNetClient>();
        if (settings.mDiagnostic) settings.mDiagnostic("RakNet connect start timeout-ms=" + std::to_string(settings.mTimeoutMs) + " outgoing-mtu-cap=1200");
        if (!transport->connect(settings.mHost, settings.mPort, settings.mTimeoutMs, settings.mCancel,
                                result.mError)) {
            if (settings.mDiagnostic) settings.mDiagnostic("RakNet connect failed: " + result.mError);
            return result;
        }
        if (settings.mDiagnostic) settings.mDiagnostic("RakNet connected; starting Bedrock login");

        connection.reset(new BedrockConnection(BedrockConnection::Side::Client, transport->getPeer(), transport));
    }

    connection->setCodecContext(settings.mCodecContext);

    LoginSequence sequence(*connection, settings, *key, std::move(authJson), std::move(clientJwt));

    if (!sequence.run(result.mError)) {
        LOG_WARN(LogAreaID::Network, "Could not connect to %s: %s", serverAddress.c_str(), result.mError.c_str());
        connection->close(result.mError);
        return result;
    }

    result.mResourcePacks = sequence.takeResourcePacks();
    result.mOfferedPacks = sequence.takeOfferedPacks();
    result.mConnection = std::move(connection);
    return result;
}
