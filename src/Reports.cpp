#include "mkqa/Reports.h"

#include <mkcontract/Files.h>
#include <mkcontract/Text.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace mkqa {
namespace {

constexpr uint32_t kLocalSig = 0x04034b50, kCentralSig = 0x02014b50, kEndSig = 0x06054b50;
constexpr uint32_t kInputBytes = 8;        // engine net::kInputBytes (per player per tick)
constexpr uint32_t kMaxPlayers = 8;        // engine net::kMaxPlayers
constexpr size_t kStampLength = 16;        // "YYYY-MM-DD_HH-MM"

uint64_t get(const uint8_t* p, int bytes) {
    uint64_t v = 0;
    for (int i = 0; i < bytes; ++i) v |= static_cast<uint64_t>(p[i]) << (8 * i);
    return v;
}

uint32_t crc32(const uint8_t* data, size_t size) {
    static uint32_t table[256] = {};
    if (!table[1])
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

bool readBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(mkcontract::pathOf(path), std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

std::string environment(const char* name) {
#if defined(_WIN32)
    char* value = nullptr;
    size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || !value) return {};
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name);
    return value ? value : "";
#endif
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// "2026-10-28_13-53[-n]" → stamp "2026-10-28 13:53" and n (1 without a suffix). false if the name isn't a stamp
bool parseStamp(const std::string& name, std::string& stamp, uint32_t& n) {
    if (name.size() < kStampLength) return false;
    static const char* pattern = "dddd-dd-dd_dd-dd";
    for (size_t i = 0; i < kStampLength; ++i)
        if (pattern[i] == 'd' ? !isDigit(name[i]) : name[i] != pattern[i]) return false;
    n = 1;
    if (name.size() > kStampLength) {
        if (name[kStampLength] != '-' || name.size() == kStampLength + 1) return false;
        n = 0;
        for (size_t i = kStampLength + 1; i < name.size(); ++i) {
            if (!isDigit(name[i])) return false;
            n = n * 10 + static_cast<uint32_t>(name[i] - '0');
        }
    }
    stamp = name.substr(0, 10) + " " + name.substr(11, 2) + ":" + name.substr(14, 2);
    return true;
}

void addFolder(std::vector<std::pair<Entry, uint32_t>>& out, const std::string& folder, const char* extension, Kind kind) {
    std::error_code ec;
    for (fs::directory_iterator it(mkcontract::pathOf(folder), ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const fs::path& p = it->path();
        if (mkcontract::utf8Of(p.extension()) != extension) continue;
        Entry e;
        e.kind = kind;
        e.path = mkcontract::utf8Of(p);
        e.name = mkcontract::utf8Of(p.stem());
        e.bytes = static_cast<uint64_t>(it->file_size(ec));
        uint32_t n = 0;
        if (!parseStamp(e.name, e.stamp, n)) e.stamp.clear();
        if (e.name == "new" || e.name == "current") continue; // the engine's half-written zip / the match being recorded
        out.emplace_back(std::move(e), n);
    }
}

std::string text(const std::vector<uint8_t>& data) { return std::string(data.begin(), data.end()); }

} // namespace

bool readZip(const std::string& path, std::vector<ZipEntry>& out, std::string& error) {
    out.clear();
    std::vector<uint8_t> bytes;
    if (!readBytes(path, bytes)) {
        error = "cannot open " + path;
        return false;
    }
    // The end record is searched from the back (a comment may follow it)
    size_t end = bytes.size() >= 22 ? bytes.size() - 22 : SIZE_MAX;
    for (; end != SIZE_MAX; --end)
        if (get(&bytes[end], 4) == kEndSig) break;
    if (end == SIZE_MAX) {
        error = "not a zip file";
        return false;
    }
    const uint32_t count = static_cast<uint32_t>(get(&bytes[end + 10], 2));
    size_t p = static_cast<size_t>(get(&bytes[end + 16], 4));
    for (uint32_t i = 0; i < count; ++i) {
        if (p + 46 > bytes.size() || get(&bytes[p], 4) != kCentralSig) {
            error = "broken zip directory";
            return false;
        }
        const uint32_t method = static_cast<uint32_t>(get(&bytes[p + 10], 2));
        const uint32_t crc = static_cast<uint32_t>(get(&bytes[p + 16], 4));
        const size_t size = static_cast<size_t>(get(&bytes[p + 20], 4));
        const size_t nameLength = static_cast<size_t>(get(&bytes[p + 28], 2));
        const size_t skip = nameLength + get(&bytes[p + 30], 2) + get(&bytes[p + 32], 2);
        const size_t local = static_cast<size_t>(get(&bytes[p + 42], 4));
        if (p + 46 + skip > bytes.size() || local + 30 > bytes.size()) {
            error = "broken zip directory";
            return false;
        }
        ZipEntry e;
        e.name.assign(reinterpret_cast<const char*>(&bytes[p + 46]), nameLength);
        p += 46 + skip;
        if (get(&bytes[local], 4) != kLocalSig) {
            error = "broken entry " + e.name;
            return false;
        }
        if (method != 0) {
            error = "compressed entry " + e.name + " (only zip files as the game wrote them are read)";
            return false;
        }
        const size_t data = local + 30 + get(&bytes[local + 26], 2) + get(&bytes[local + 28], 2);
        if (data + size > bytes.size() || crc32(&bytes[data], size) != crc) {
            error = "broken entry " + e.name;
            return false;
        }
        e.data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(data), bytes.begin() + static_cast<std::ptrdiff_t>(data + size));
        out.push_back(std::move(e));
    }
    return true;
}

Recording readRecording(const std::vector<uint8_t>& bytes) {
    Recording r;
    constexpr size_t kHeaderBytes = 8 + 4 + 4 + 8 + 4 + 4 + 8;
    if (bytes.size() < kHeaderBytes || std::memcmp(bytes.data(), "MKINPUT1", 8) != 0) {
        r.error = "not a MealKit input recording";
        return r;
    }
    const uint8_t* p = bytes.data() + 8;
    const uint8_t* end = bytes.data() + bytes.size();
    r.version = static_cast<uint32_t>(get(p, 4));
    r.players = static_cast<uint32_t>(get(p + 4, 4));
    r.seed = get(p + 8, 8);
    r.contentHash = get(p + 24, 8);
    p += 32;
    if (r.version < 1 || r.version > 3) {
        r.error = "unsupported recording version " + std::to_string(r.version);
        return r;
    }
    if (r.players == 0 || r.players > kMaxPlayers) {
        r.error = "bad player count";
        return r;
    }
    if (r.version >= 3)
        for (uint32_t i = 0; i < r.players; ++i) { // loadouts: 1 byte size + bytes
            if (p >= end || static_cast<size_t>(end - p) < 1u + *p) {
                r.error = "truncated loadouts";
                return r;
            }
            p += 1 + *p;
        }
    const size_t inputs = r.players * kInputBytes;
    for (;;) {
        if (static_cast<size_t>(end - p) < inputs + (r.version >= 2 ? 2u : 0u) + 8u) break;
        p += inputs;
        if (r.version >= 2) {
            const size_t commands = static_cast<size_t>(get(p, 2));
            if (static_cast<size_t>(end - p) < 2 + commands + 8) break;
            p += 2 + commands;
        }
        p += 8; // state hash
        ++r.ticks;
    }
    r.valid = true;
    return r;
}

const char* kindName(Kind kind) {
    switch (kind) {
    case Kind::Bug: return "bug";
    case Kind::Crash: return "crash";
    case Kind::Replay: return "replay";
    }
    return "?";
}

std::vector<Entry> listEntries(const std::string& gameFolder) {
    std::vector<std::pair<Entry, uint32_t>> all;
    addFolder(all, mkcontract::joinPath(gameFolder, "bugreports"), ".zip", Kind::Bug);
    addFolder(all, mkcontract::joinPath(gameFolder, "crash"), ".zip", Kind::Crash);
    addFolder(all, mkcontract::joinPath(gameFolder, "replays"), ".rec", Kind::Replay);
    std::stable_sort(all.begin(), all.end(), [](const auto& a, const auto& b) {
        const std::string sa = a.first.name.substr(0, std::min(a.first.name.size(), kStampLength));
        const std::string sb = b.first.name.substr(0, std::min(b.first.name.size(), kStampLength));
        if (sa != sb) return sa > sb;
        if (a.second != b.second) return a.second > b.second;
        return a.first.kind < b.first.kind;
    });
    std::vector<Entry> out;
    for (auto& [e, n] : all) out.push_back(std::move(e));
    return out;
}

std::string Report::value(const std::string& key) const {
    for (const auto& [k, v] : info)
        if (k == key) return v;
    return {};
}

std::string Report::summaryText() const {
    std::string s = (header.empty() ? std::string(kindName(entry.kind)) : header) + ": " + entry.path + "\n";
    for (const auto& [k, v] : info) s += k + " = " + v + "\n";
    if (recording.valid)
        s += "recording = " + std::to_string(recording.players) + " player(s), " + std::to_string(recording.ticks) + " tick(s)\n";
    if (!crash.empty()) s += "\n" + crash + (crash.back() == '\n' ? "" : "\n");
    return s;
}

bool loadReport(const Entry& entry, Report& out) {
    out = Report{};
    out.entry = entry;
    if (entry.kind == Kind::Replay) {
        std::vector<uint8_t> bytes;
        if (!readBytes(entry.path, bytes)) {
            out.error = "cannot open " + entry.path;
            return false;
        }
        out.recording = readRecording(bytes);
        if (!out.recording.valid) out.error = out.recording.error;
        return out.recording.valid;
    }
    std::vector<ZipEntry> files;
    if (!readZip(entry.path, files, out.error)) return false;
    for (ZipEntry& f : files) {
        if (f.name == "info.txt") {
            for (const std::string& line : mkcontract::splitLines(text(f.data))) {
                const std::string t = mkcontract::trim(line);
                if (t.empty()) continue;
                if (t[0] == '#') {
                    if (out.header.empty()) out.header = mkcontract::trim(t.substr(1));
                    continue;
                }
                const size_t eq = t.find('=');
                if (eq == std::string::npos) continue;
                out.info.emplace_back(mkcontract::trim(t.substr(0, eq)), mkcontract::trim(t.substr(eq + 1)));
            }
        } else if (f.name == "log.txt") {
            out.log = text(f.data);
        } else if (f.name == "crash.txt") {
            out.crash = text(f.data);
        } else if (f.name == "screenshot.png") {
            out.screenshot = std::move(f.data);
        } else if (f.name == "crash.dmp") {
            out.hasDump = true;
            out.dumpBytes = f.data.size();
        } else if (f.name == "replay.rec") {
            out.recording = readRecording(f.data);
        }
    }
    return true;
}

bool readProject(const std::string& directory, Project& out) {
    out = Project{};
    out.directory = mkcontract::normalizeDirectory(directory);
    const std::string file = mkcontract::joinPath(out.directory, "mealkit.project");
    if (!mkcontract::pathExists(file)) return false;
    out.name = mkcontract::readKey(file, "name");
    out.engine = mkcontract::readKey(file, "engine");
    out.engine = out.engine.empty() ? out.directory : mkcontract::normalizeDirectory(out.engine);
    const std::string rate = mkcontract::readKey(file, "tickRate");
    if (!rate.empty()) out.tickRate = static_cast<uint32_t>(std::strtoul(rate.c_str(), nullptr, 10));
    if (out.tickRate == 0) out.tickRate = 30;
    out.valid = true;
    return true;
}

std::string gameFolder(const std::string& name) {
    const std::string overridden = environment("MEALKIT_GAME_DIR");
    if (!overridden.empty()) return overridden;
#if defined(_WIN32)
    std::string base = environment("LOCALAPPDATA");
#else
    const std::string home = environment("HOME");
#if defined(__APPLE__)
    std::string base = home.empty() ? std::string() : home + "/Library/Application Support";
#else
    std::string base = environment("XDG_DATA_HOME");
    if (base.empty() && !home.empty()) base = home + "/.local/share";
#endif
#endif
    if (base.empty()) return {};
    return mkcontract::joinPath(base, name.empty() ? std::string("MealKitGame") : name);
}

std::string editorExe(const std::string& engineDirectory) {
#if defined(_WIN32)
    const char* exe = "MealKitEditor.exe";
#else
    const char* exe = "MealKitEditor";
#endif
    const std::string path = mkcontract::utf8Of(mkcontract::pathOf(engineDirectory) / "build" / "Editor" / "bin" / exe);
    return mkcontract::pathExists(path) ? path : std::string();
}

std::vector<std::string> editorReplayArgs(const std::string& editor, const Project& project, const std::string& file) {
    return {editor, "--project", project.directory, "--replay", file};
}

bool removeEntry(const Entry& entry, std::string& error) {
    std::error_code ec;
    if (!fs::remove(mkcontract::pathOf(entry.path), ec)) {
        error = ec ? ec.message() : "no such file";
        return false;
    }
    return true;
}

} // namespace mkqa
