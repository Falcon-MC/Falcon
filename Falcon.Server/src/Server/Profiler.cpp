#include "Server/Profiler.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace {
    const char *SECTION_NAMES[(int) ProfilerSection::Count] = {
            "Console commands",
            "Network drain",
            "LevelChunk drain",
            "Fluid physics",
            "Player tick",
            "LevelChunk streaming",
            "Block actors",
            "Item actors",
            "Announcement",
            "Fluid broadcast",
            "Redstone",
            "Actor physics",
            "Actor environment",
            "Actor projectiles",
            "Actor persistence",
            "Sky light",
            "Weather",
            "Network pump io",
            "Network connection",
            "Network decode",
            "Network handle packet",
            "Fire",
            "LevelChunk population"
    };

    const char *SECTION_COLORS[(int) ProfilerSection::Count] = {
            "#8b8b8b",
            "#4f9df7",
            "#7b61ff",
            "#00b8d4",
            "#ffb020",
            "#2dd4a7",
            "#f2622e",
            "#e0567c",
            "#9aa4b2",
            "#c084fc",
            "#e02f2f",
            "#5ed36a",
            "#37a86b",
            "#8fd94f",
            "#d98c00",
            "#7cc4ff",
            "#a0a0ff",
            "#1f6feb",
            "#79b8ff",
            "#b392f0",
            "#f97583",
            "#ff7043",
            "#00c2a8"
    };

    bool createDirectory(const std::string &path) {
#ifdef _WIN32
        return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
    }

    std::string timestampSuffix() {
        const std::time_t now = std::time(nullptr);
        std::tm parts{};
#ifdef _WIN32
        localtime_s(&parts, &now);
#else
        localtime_r(&now, &parts);
#endif

        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", &parts);
        return std::string(buffer);
    }

    std::string escapeHtml(const std::string &value) {
        std::string result;
        result.reserve(value.size());

        for (char character: value) {
            switch (character) {
                case '&':
                    result += "&amp;";
                    break;
                case '<':
                    result += "&lt;";
                    break;
                case '>':
                    result += "&gt;";
                    break;
                case '"':
                    result += "&quot;";
                    break;
                default:
                    result.push_back(character);
                    break;
            }
        }

        return result;
    }

    std::string formatNumber(double value, int precision = 2) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision) << value;
        return stream.str();
    }
}

const char *getProfilerSectionName(ProfilerSection section) {
    const int index = (int) section;
    if (index < 0 || index >= (int) ProfilerSection::Count)
        return "Unknown";

    return SECTION_NAMES[index];
}

ProfilerScopedSection::ProfilerScopedSection(Profiler &profiler, ProfilerSection section, bool active)
        : mProfiler(profiler), mSection(section), mActive(active) {
    if (mActive)
        mProfiler.beginSection(mSection);
}

ProfilerScopedSection::~ProfilerScopedSection() {
    if (mActive)
        mProfiler.endSection(mSection);
}

void Profiler::start(int64_t tick) {
    mActive = true;
    mTickStarted = false;
    mStartTick = tick;
    mStartTime = std::chrono::steady_clock::now();
    mSamples.clear();
    mPlayers.clear();
    mCurrent = ProfilerTickSample();
}

void Profiler::beginTick(int64_t tick) {
    if (!mActive)
        return;

    mCurrent = ProfilerTickSample();
    mCurrent.mTick = tick;
    mTickStart = std::chrono::steady_clock::now();
    mTickStarted = true;
}

void Profiler::endTick(uint32_t playerCount, uint32_t loadedChunks, uint32_t pendingChunkTasks,
                       uint32_t fluidProcessed, uint32_t fluidScheduled) {
    if (!mActive || !mTickStarted || mSamples.size() >= MAX_SAMPLES)
        return;

    mCurrent.mTotalMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - mTickStart).count();
    mCurrent.mPlayerCount = playerCount;
    mCurrent.mLoadedChunks = loadedChunks;
    mCurrent.mPendingChunkTasks = pendingChunkTasks;
    mCurrent.mFluidProcessed = fluidProcessed;
    mCurrent.mFluidScheduled = fluidScheduled;

    mSamples.push_back(mCurrent);
}

void Profiler::beginSection(ProfilerSection section) {
    if (!mActive)
        return;

    mSectionStart[(int) section] = std::chrono::steady_clock::now();
}

void Profiler::endSection(ProfilerSection section) {
    if (!mActive)
        return;

    const int index = (int) section;
    mCurrent.mSectionMs[index] += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - mSectionStart[index]).count();
}

void Profiler::recordPlayer(const std::string &name, double milliseconds, uint32_t chunksSent) {
    if (!mActive)
        return;

    ProfilerPlayerStats &stats = mPlayers[name];
    stats.mName = name;
    stats.mTotalMs += milliseconds;
    stats.mTicks++;
    stats.mChunksSent += chunksSent;

    if (milliseconds > stats.mPeakMs)
        stats.mPeakMs = milliseconds;
}

bool Profiler::stop(const std::string &directory, std::string &outPath, std::string &outError) {
    if (!mActive) {
        outError = "The profiler is not running";
        return false;
    }

    mActive = false;
    mStopTime = std::chrono::steady_clock::now();

    if (mSamples.empty()) {
        outError = "No tick was recorded";
        return false;
    }

    if (!createDirectory(directory)) {
        outError = "Could not create the directory " + directory;
        return false;
    }

    const std::string path = directory + "/profile-" + timestampSuffix() + ".html";

    std::ofstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        outError = "Could not write " + path;
        return false;
    }

    const std::string report = _buildReport();
    file.write(report.data(), (std::streamsize) report.size());
    file.close();

    outPath = path;
    return true;
}

std::string Profiler::_buildReport() const {
    const double durationSeconds = std::chrono::duration<double>(mStopTime - mStartTime).count();

    double totalMs = 0.0;
    double peakMs = 0.0;
    double minMs = mSamples.front().mTotalMs;
    double sectionTotals[(int) ProfilerSection::Count] = {};
    double sectionPeaks[(int) ProfilerSection::Count] = {};
    uint32_t peakPending = 0;

    for (const ProfilerTickSample &sample: mSamples) {
        totalMs += sample.mTotalMs;
        peakMs = std::max(peakMs, sample.mTotalMs);
        minMs = std::min(minMs, sample.mTotalMs);
        peakPending = std::max(peakPending, sample.mPendingChunkTasks);

        for (int i = 0; i < (int) ProfilerSection::Count; ++i) {
            sectionTotals[i] += sample.mSectionMs[i];
            sectionPeaks[i] = std::max(sectionPeaks[i], sample.mSectionMs[i]);
        }
    }

    const double sampleCount = (double) mSamples.size();
    const double averageMs = totalMs / sampleCount;
    const double averageTps = durationSeconds > 0.0 ? std::min(20.0, sampleCount / durationSeconds) : 20.0;

    std::vector<ProfilerPlayerStats> players;
    players.reserve(mPlayers.size());
    for (const auto &entry: mPlayers)
        players.push_back(entry.second);

    std::sort(players.begin(), players.end(), [](const ProfilerPlayerStats &left, const ProfilerPlayerStats &right) {
        return left.mTotalMs > right.mTotalMs;
    });

    std::vector<int> sectionOrder;
    for (int i = 0; i < (int) ProfilerSection::Count; ++i)
        sectionOrder.push_back(i);

    std::sort(sectionOrder.begin(), sectionOrder.end(), [&sectionTotals](int left, int right) {
        return sectionTotals[left] > sectionTotals[right];
    });

    std::ostringstream out;

    out << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        << "<title>Falcon profiler report</title><style>"
        << ":root{--bg:#0f1115;--panel:#171a21;--line:#242833;--text:#e6e9ef;--muted:#9aa4b2;--good:#2dd4a7;"
        << "--warn:#ffb020;--bad:#f2622e;}"
        << "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);"
        << "font-family:ui-sans-serif,system-ui,'Segoe UI',Roboto,sans-serif;line-height:1.5}"
        << ".wrap{max-width:1180px;margin:0 auto;padding:32px 20px 64px}"
        << "h1{font-size:26px;margin:0 0 4px}h2{font-size:17px;margin:32px 0 12px;font-weight:600}"
        << ".sub{color:var(--muted);font-size:14px;margin-bottom:24px}"
        << ".cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(168px,1fr));gap:12px}"
        << ".card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:14px 16px}"
        << ".card .k{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.05em}"
        << ".card .v{font-size:22px;font-weight:600;margin-top:4px;font-variant-numeric:tabular-nums}"
        << ".panel{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:16px;overflow-x:auto}"
        << "table{width:100%;border-collapse:collapse;font-size:14px;min-width:520px}"
        << "th,td{text-align:left;padding:9px 10px;border-bottom:1px solid var(--line)}"
        << "th{color:var(--muted);font-weight:600;font-size:12px;text-transform:uppercase;letter-spacing:.04em}"
        << "td.n,th.n{text-align:right;font-variant-numeric:tabular-nums}"
        << "tr:last-child td{border-bottom:none}"
        << ".sw{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:8px;vertical-align:middle}"
        << ".bar{height:6px;border-radius:3px;background:var(--line);overflow:hidden;margin-top:6px}"
        << ".bar>i{display:block;height:100%}"
        << ".good{color:var(--good)}.warn{color:var(--warn)}.bad{color:var(--bad)}"
        << ".legend{display:flex;flex-wrap:wrap;gap:14px;margin-top:12px;font-size:13px;color:var(--muted)}"
        << "</style></head><body><div class=\"wrap\">";

    out << "<h1>Falcon profiler report</h1>"
        << "<div class=\"sub\">" << formatNumber(durationSeconds, 1) << " s &middot; "
        << mSamples.size() << " ticks recorded &middot; starting at tick " << mStartTick << "</div>";

    const char *msptClass = averageMs < 35.0 ? "good" : (averageMs < 50.0 ? "warn" : "bad");
    const char *tpsClass = averageTps > 19.5 ? "good" : (averageTps > 17.0 ? "warn" : "bad");

    out << "<div class=\"cards\">"
        << "<div class=\"card\"><div class=\"k\">Average TPS</div><div class=\"v " << tpsClass << "\">"
        << formatNumber(averageTps) << "</div></div>"
        << "<div class=\"card\"><div class=\"k\">Average MSPT</div><div class=\"v " << msptClass << "\">"
        << formatNumber(averageMs) << " ms</div></div>"
        << "<div class=\"card\"><div class=\"k\">Peak MSPT</div><div class=\"v\">"
        << formatNumber(peakMs) << " ms</div></div>"
        << "<div class=\"card\"><div class=\"k\">Lowest MSPT</div><div class=\"v\">"
        << formatNumber(minMs) << " ms</div></div>"
        << "<div class=\"card\"><div class=\"k\">Budget used</div><div class=\"v\">"
        << formatNumber(averageMs / 50.0 * 100.0, 1) << " %</div></div>"
        << "<div class=\"card\"><div class=\"k\">Peak chunk queue</div><div class=\"v\">"
        << peakPending << "</div></div>"
        << "</div>";

    out << "<h2>Tick duration</h2><div class=\"panel\">";

    const double chartMax = std::max(peakMs, 50.0) * 1.1;
    const double chartWidth = 1100.0;
    const double chartHeight = 260.0;

    out << "<svg viewBox=\"0 0 " << (int) chartWidth << " " << (int) chartHeight
        << "\" width=\"100%\" height=\"260\" preserveAspectRatio=\"none\">";

    const double budgetY = chartHeight - (50.0 / chartMax) * chartHeight;
    out << "<line x1=\"0\" y1=\"" << formatNumber(budgetY, 1) << "\" x2=\"" << (int) chartWidth
        << "\" y2=\"" << formatNumber(budgetY, 1)
        << "\" stroke=\"#f2622e\" stroke-width=\"1\" stroke-dasharray=\"4 4\"/>";
    out << "<text x=\"6\" y=\"" << formatNumber(budgetY - 6.0, 1)
        << "\" fill=\"#f2622e\" font-size=\"11\">50 ms budget (20 TPS)</text>";

    out << "<polyline fill=\"none\" stroke=\"#4f9df7\" stroke-width=\"1.5\" points=\"";
    for (size_t i = 0; i < mSamples.size(); ++i) {
        const double x = sampleCount <= 1.0 ? 0.0 : (double) i / (sampleCount - 1.0) * chartWidth;
        const double y = chartHeight - std::min(1.0, mSamples[i].mTotalMs / chartMax) * chartHeight;
        out << formatNumber(x, 1) << "," << formatNumber(y, 1) << " ";
    }
    out << "\"/></svg>";

    out << "<div class=\"legend\"><span>Blue line: tick duration in milliseconds</span>"
        << "<span>Y axis peak: " << formatNumber(chartMax) << " ms</span></div></div>";

    out << "<h2>Where the time goes</h2><div class=\"panel\"><table>"
        << "<thead><tr><th>Section</th><th class=\"n\">Average</th><th class=\"n\">Peak</th>"
        << "<th class=\"n\">Total</th><th class=\"n\">Share</th></tr></thead><tbody>";

    for (int index: sectionOrder) {
        const double average = sectionTotals[index] / sampleCount;
        const double share = totalMs > 0.0 ? sectionTotals[index] / totalMs * 100.0 : 0.0;

        out << "<tr><td><span class=\"sw\" style=\"background:" << SECTION_COLORS[index] << "\"></span>"
            << escapeHtml(SECTION_NAMES[index]) << "</td>"
            << "<td class=\"n\">" << formatNumber(average, 3) << " ms</td>"
            << "<td class=\"n\">" << formatNumber(sectionPeaks[index], 3) << " ms</td>"
            << "<td class=\"n\">" << formatNumber(sectionTotals[index], 1) << " ms</td>"
            << "<td class=\"n\">" << formatNumber(share, 1) << " %"
            << "<div class=\"bar\"><i style=\"width:" << formatNumber(share, 1)
            << "%;background:" << SECTION_COLORS[index] << "\"></i></div></td></tr>";
    }

    const double accounted = std::max(0.0, totalMs - [&sectionTotals]() {
        double sum = 0.0;
        for (int i = 0; i < (int) ProfilerSection::Count; ++i)
            sum += sectionTotals[i];
        return sum;
    }());

    out << "<tr><td><span class=\"sw\" style=\"background:#3a3f4b\"></span>Unaccounted</td>"
        << "<td class=\"n\">" << formatNumber(accounted / sampleCount, 3) << " ms</td>"
        << "<td class=\"n\">-</td>"
        << "<td class=\"n\">" << formatNumber(accounted, 1) << " ms</td>"
        << "<td class=\"n\">" << formatNumber(totalMs > 0.0 ? accounted / totalMs * 100.0 : 0.0, 1)
        << " %</td></tr>";

    out << "</tbody></table></div>";

    out << "<h2>Players</h2><div class=\"panel\">";

    if (players.empty()) {
        out << "<div class=\"sub\" style=\"margin:0\">No player was ticked during the capture.</div>";
    } else {
        out << "<table><thead><tr><th>Player</th><th class=\"n\">Total</th><th class=\"n\">Average / tick</th>"
            << "<th class=\"n\">Peak</th><th class=\"n\">Ticks</th><th class=\"n\">Chunks sent</th>"
            << "</tr></thead><tbody>";

        for (const ProfilerPlayerStats &player: players) {
            const double average = player.mTicks == 0 ? 0.0 : player.mTotalMs / (double) player.mTicks;

            out << "<tr><td>" << escapeHtml(player.mName) << "</td>"
                << "<td class=\"n\">" << formatNumber(player.mTotalMs, 1) << " ms</td>"
                << "<td class=\"n\">" << formatNumber(average, 3) << " ms</td>"
                << "<td class=\"n\">" << formatNumber(player.mPeakMs, 3) << " ms</td>"
                << "<td class=\"n\">" << player.mTicks << "</td>"
                << "<td class=\"n\">" << player.mChunksSent << "</td></tr>";
        }

        out << "</tbody></table>";
    }

    out << "</div>";

    out << "<h2>Slowest ticks</h2><div class=\"panel\"><table>"
        << "<thead><tr><th class=\"n\">Tick</th><th class=\"n\">Duration</th><th>Dominant section</th>"
        << "<th class=\"n\">Players</th><th class=\"n\">Chunks</th><th class=\"n\">LevelChunk queue</th>"
        << "<th class=\"n\">Fluid done</th><th class=\"n\">Fluid queue</th>"
        << "</tr></thead><tbody>";

    std::vector<const ProfilerTickSample *> slowest;
    slowest.reserve(mSamples.size());
    for (const ProfilerTickSample &sample: mSamples)
        slowest.push_back(&sample);

    std::sort(slowest.begin(), slowest.end(), [](const ProfilerTickSample *left, const ProfilerTickSample *right) {
        return left->mTotalMs > right->mTotalMs;
    });

    const size_t slowestCount = std::min<size_t>(15, slowest.size());
    for (size_t i = 0; i < slowestCount; ++i) {
        const ProfilerTickSample &sample = *slowest[i];

        int dominant = 0;
        double measuredMs = 0.0;
        for (int index = 0; index < (int) ProfilerSection::Count; ++index) {
            measuredMs += sample.mSectionMs[index];
            if (sample.mSectionMs[index] > sample.mSectionMs[dominant])
                dominant = index;
        }

        const double unaccountedMs = sample.mTotalMs - measuredMs;
        const bool unaccountedDominates = unaccountedMs > sample.mSectionMs[dominant];

        out << "<tr><td class=\"n\">" << sample.mTick << "</td>"
            << "<td class=\"n\">" << formatNumber(sample.mTotalMs, 2) << " ms</td>";

        if (unaccountedDominates) {
            out << "<td><span class=\"sw\" style=\"background:#ff2d55\"></span>Unaccounted ("
                << formatNumber(unaccountedMs, 2) << " ms)</td>";
        } else {
            out << "<td><span class=\"sw\" style=\"background:" << SECTION_COLORS[dominant] << "\"></span>"
                << escapeHtml(SECTION_NAMES[dominant]) << " ("
                << formatNumber(sample.mSectionMs[dominant], 2) << " ms)</td>";
        }

        out
            << "<td class=\"n\">" << sample.mPlayerCount << "</td>"
            << "<td class=\"n\">" << sample.mLoadedChunks << "</td>"
            << "<td class=\"n\">" << sample.mPendingChunkTasks << "</td>"
            << "<td class=\"n\">" << sample.mFluidProcessed << "</td>"
            << "<td class=\"n\">" << sample.mFluidScheduled << "</td></tr>";
    }

    out << "</tbody></table></div>";

    out << "</div></body></html>";

    return out.str();
}
