// mkqa tests: zip reading, recordings, the report list and reports, projects. Builds a fake game folder in the temp folder.
#include "Strings.h"
#include "mkqa/Reports.h"
#include "mkqa/Tests.h"

#include <mkcontract/Files.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace mkqa;

namespace {

int g_failures = 0;
#define CHECK(x)                                                                \
    do {                                                                        \
        if (!(x)) {                                                             \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);          \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

std::string str(const fs::path& p) { return p.string(); }

void put(std::vector<uint8_t>& out, uint64_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

uint32_t crc32(const std::vector<uint8_t>& d) {
    uint32_t c = 0xFFFFFFFFu;
    for (uint8_t b : d) {
        c ^= b;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    return c ^ 0xFFFFFFFFu;
}

// Stored zip like the engine's core/Zip.cpp writes (method: 0 stored, 8 to fake a compressed entry)
std::vector<uint8_t> zip(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files, uint16_t method = 0) {
    std::vector<uint8_t> out, central;
    for (const auto& [name, data] : files) {
        const uint32_t offset = static_cast<uint32_t>(out.size());
        const uint32_t crc = crc32(data);
        const uint32_t size = static_cast<uint32_t>(data.size());
        put(out, 0x04034b50, 4);
        put(out, 20, 2);
        put(out, 0x0800, 2);
        put(out, method, 2);
        put(out, 0, 4);
        put(out, crc, 4);
        put(out, size, 4);
        put(out, size, 4);
        put(out, name.size(), 2);
        put(out, 0, 2);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());
        put(central, 0x02014b50, 4);
        put(central, 20, 2);
        put(central, 20, 2);
        put(central, 0x0800, 2);
        put(central, method, 2);
        put(central, 0, 4);
        put(central, crc, 4);
        put(central, size, 4);
        put(central, size, 4);
        put(central, name.size(), 2);
        put(central, 0, 2);
        put(central, 0, 2);
        put(central, 0, 2);
        put(central, 0, 2);
        put(central, 0, 4);
        put(central, offset, 4);
        central.insert(central.end(), name.begin(), name.end());
    }
    const uint32_t centralOffset = static_cast<uint32_t>(out.size());
    out.insert(out.end(), central.begin(), central.end());
    put(out, 0x06054b50, 4);
    put(out, 0, 4);
    put(out, files.size(), 2);
    put(out, files.size(), 2);
    put(out, central.size(), 4);
    put(out, centralOffset, 4);
    put(out, 0, 2);
    return out;
}

// Input recording version 3: players, seed, ticks with commandBytes of commands each
std::vector<uint8_t> recording(uint32_t players, uint64_t seed, uint32_t ticks, uint32_t commandBytes = 0) {
    std::vector<uint8_t> out = {'M', 'K', 'I', 'N', 'P', 'U', 'T', '1'};
    put(out, 3, 4);
    put(out, players, 4);
    put(out, seed, 8);
    put(out, 0, 4);
    put(out, 0, 4);
    put(out, 0xABCDEFull, 8);
    for (uint32_t p = 0; p < players; ++p) {
        put(out, 2, 1); // 2-byte loadout
        put(out, p, 2);
    }
    for (uint32_t t = 0; t < ticks; ++t) {
        out.insert(out.end(), players * 8, static_cast<uint8_t>(t));
        put(out, commandBytes, 2);
        out.insert(out.end(), commandBytes, 7);
        put(out, t * 31, 8);
    }
    return out;
}

std::vector<uint8_t> bytes(const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); }

void write(const fs::path& path, const std::vector<uint8_t>& data) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

void testRecording() {
    Recording r = readRecording(recording(2, 12345, 10, 3));
    CHECK(r.valid);
    CHECK(r.version == 3);
    CHECK(r.players == 2);
    CHECK(r.seed == 12345);
    CHECK(r.contentHash == 0xABCDEF);
    CHECK(r.ticks == 10);
    std::vector<uint8_t> cut = recording(1, 1, 5);
    cut.resize(cut.size() - 3); // half-written last tick (game still running / crashed) is not counted
    CHECK(readRecording(cut).ticks == 4);
    CHECK(!readRecording(bytes("hello")).valid);
    std::vector<uint8_t> bad = recording(1, 1, 1);
    bad[12] = 9; // 9 players
    CHECK(!readRecording(bad).valid);
}

void testZip(const fs::path& root) {
    write(root / "a.zip", zip({{"x.txt", bytes("hello")}, {"y.bin", {1, 2, 3}}}));
    std::vector<ZipEntry> files;
    std::string error;
    CHECK(readZip(str(root / "a.zip"), files, error));
    CHECK(files.size() == 2 && files[0].name == "x.txt" && files[1].data.size() == 3);
    write(root / "b.zip", zip({{"x.txt", bytes("hello")}}, 8));
    CHECK(!readZip(str(root / "b.zip"), files, error));
    CHECK(error.find("compressed") != std::string::npos);
    std::vector<uint8_t> broken = zip({{"x.txt", bytes("hello")}});
    broken[36] ^= 0xFF; // data byte (30-byte header + "x.txt") → CRC mismatch
    write(root / "c.zip", broken);
    CHECK(!readZip(str(root / "c.zip"), files, error));
    write(root / "d.zip", bytes("not a zip"));
    CHECK(!readZip(str(root / "d.zip"), files, error));
}

void testReports(const fs::path& root) {
    const fs::path game = root / "game";
    const std::string info = "# MealKit bug report\nengine = 0.7.2\ncommit = abc\nscene = scenes/arena.scene\nnote = falls through the floor\ntick = 300\n"
                             "players = 2\n";
    write(game / "bugreports" / "2026-10-28_13-53.zip",
          zip({{"info.txt", bytes(info)}, {"replay.rec", recording(2, 7, 300)}, {"log.txt", bytes("line 1\nline 2\n")},
               {"screenshot.png", {0x89, 'P', 'N', 'G'}}}));
    write(game / "bugreports" / "2026-10-28_13-53-2.zip", zip({{"info.txt", bytes("# MealKit bug report\nnote = second\n")}}));
    write(game / "bugreports" / "new.zip", zip({}));
    write(game / "crash" / "2026-11-01_09-00.zip",
          zip({{"info.txt", bytes("# MealKit crash report\ntick = 12\n")}, {"crash.txt", bytes("exception = c0000005\n")},
               {"crash.dmp", std::vector<uint8_t>(100, 0)}, {"replay.rec", recording(1, 1, 12)}}));
    write(game / "replays" / "2026-10-29_20-47.rec", recording(1, 5, 60));
    write(game / "replays" / "current.rec", recording(1, 5, 1));
    write(game / "replays" / "notes.txt", bytes("x"));

    const std::vector<Entry> list = listEntries(str(game));
    CHECK(list.size() == 4);
    if (list.size() == 4) {
        CHECK(list[0].kind == Kind::Crash && list[0].stamp == "2026-11-01 09:00");
        CHECK(list[1].kind == Kind::Replay);
        CHECK(list[2].name == "2026-10-28_13-53-2"); // same minute: -2 is newer
        CHECK(list[3].name == "2026-10-28_13-53");
    }
    CHECK(listEntries(str(root / "missing")).empty());

    Report r;
    CHECK(list.size() == 4 && loadReport(list[3], r));
    CHECK(r.header == "MealKit bug report");
    CHECK(r.value("note") == "falls through the floor");
    CHECK(r.value("tick") == "300");
    CHECK(r.value("nothing").empty());
    CHECK(r.log == "line 1\nline 2\n");
    CHECK(r.screenshot.size() == 4);
    CHECK(r.recording.valid && r.recording.ticks == 300 && r.recording.players == 2);
    CHECK(r.summaryText().find("note = falls through the floor") != std::string::npos);

    CHECK(list.size() == 4 && loadReport(list[0], r));
    CHECK(r.hasDump && r.dumpBytes == 100);
    CHECK(r.crash == "exception = c0000005\n");
    CHECK(r.summaryText().find("exception = c0000005") != std::string::npos);

    CHECK(list.size() == 4 && loadReport(list[1], r));
    CHECK(r.recording.valid && r.recording.ticks == 60 && r.info.empty());

    std::string error;
    CHECK(list.size() == 4 && removeEntry(list[2], error));
    CHECK(listEntries(str(game)).size() == 3);
    CHECK(list.size() == 4 && !removeEntry(list[2], error));
}

void testProject(const fs::path& root) {
    const fs::path p = root / "proj";
    write(p / "mealkit.project", bytes("# x\n@format project 1\nname mygame\nengine " + str(root / "engine") + "\ntickRate 60\n"));
    Project project;
    CHECK(readProject(str(p), project));
    CHECK(project.valid && project.name == "mygame" && project.tickRate == 60);
    CHECK(mkcontract::sameDirectory(project.engine, str(root / "engine")));
    CHECK(editorExe(project.engine).empty());
#if defined(_WIN32)
    write(root / "engine" / "build" / "Editor" / "bin" / "MealKitEditor.exe", bytes("x"));
#else
    write(root / "engine" / "build" / "Editor" / "bin" / "MealKitEditor", bytes("x"));
#endif
    const std::string editor = editorExe(project.engine);
    CHECK(!editor.empty());
    const std::vector<std::string> args = editorReplayArgs(editor, project, "r.zip");
    CHECK(args.size() == 5 && args[1] == "--project" && args[4] == "r.zip");

    write(root / "repo" / "mealkit.project", bytes("name sandbox\n"));
    CHECK(readProject(str(root / "repo"), project));
    CHECK(project.tickRate == 30);
    CHECK(mkcontract::sameDirectory(project.engine, project.directory)); // no engine line: the engine repo itself
    CHECK(!readProject(str(root / "nothing"), project));

    CHECK(fs::path(gameFolder("mygame")).filename() == "mygame" || std::getenv("MEALKIT_GAME_DIR"));
    CHECK(fs::path(gameFolder("")).filename() == "MealKitGame" || std::getenv("MEALKIT_GAME_DIR"));
}

void testTests(const fs::path& root) {
    // Build root: a project with build.bat, an engine sample (samples/<name> of a repo with engine/src/mealkit), else the folder
    const fs::path engine = root / "eng";
    write(engine / "build.bat", bytes("x"));
    write(engine / "engine" / "src" / "mealkit" / "x.h", bytes("x"));
    write(engine / "samples" / "demo" / "mealkit.project", bytes("name demo\n"));
    Project sample;
    CHECK(readProject(str(engine / "samples" / "demo"), sample));
    CHECK(mkcontract::sameDirectory(buildRoot(sample), str(engine)));
    CHECK(headlessExe(sample).empty());
#if defined(_WIN32)
    write(engine / "build" / "Release" / "bin" / "demo_headless.exe", bytes("x"));
#else
    write(engine / "build" / "Release" / "bin" / "demo_headless", bytes("x"));
#endif
    CHECK(!headlessExe(sample).empty());
    const std::vector<std::string> mp = mpTestArgs(sample, 3, 900);
    CHECK(mp.size() > 6 && mp[mp.size() - 1] == "900" && mp[mp.size() - 3] == "3");
    CHECK(mp.end() != std::find_if(mp.begin(), mp.end(), [&](const std::string& a) {
              return mkcontract::sameDirectory(fs::path(a).parent_path().parent_path().string(), str(engine));
          })); // the engine's tools/mp_test.ps1
    Project plain;
    write(root / "plain" / "mealkit.project", bytes("name plain\n"));
    CHECK(readProject(str(root / "plain"), plain) && mkcontract::sameDirectory(buildRoot(plain), plain.directory));

    // Scenario list and output
    write(engine / "samples" / "demo" / "tests" / "b.scenario", bytes("tick 1 x == 1\n"));
    write(engine / "samples" / "demo" / "tests" / "a.scenario", bytes("tick 1 x == 1\n"));
    write(engine / "samples" / "demo" / "tests" / "a.rec", bytes("x"));
    const std::vector<std::string> list = listScenarios(sample);
    CHECK(list.size() == 2 && fs::path(list[0]).filename() == "a.scenario");
    ScenarioResult r = parseScenarioOutput("[ 0.1][WARN ] x\nSCENARIO ok a.scenario checks=4\n", 0);
    CHECK(r.state == ScenarioResult::State::Passed && r.checks == 4 && r.failures.empty());
    r = parseScenarioOutput("SCENARIO FAIL a.scenario:3 tick 5 x == 1 (got 2)\nSCENARIO FAIL a.scenario failed=1/4\n", 6);
    CHECK(r.state == ScenarioResult::State::Failed && r.checks == 4 && r.failed == 1 && r.failures.size() == 1);
    CHECK(r.failures.size() == 1 && r.failures[0] == "a.scenario:3 tick 5 x == 1 (got 2)");
    r = parseScenarioOutput("[ERROR] scene not found\n", 1);
    CHECK(r.state == ScenarioResult::State::Failed && r.failures.size() == 1);
    CHECK(scenarioArgs("h", "a", true).back() == "--update-expected" && scenarioArgs("h", "a", false).size() == 3);
    MpTestResult m = parseMpTestOutput("x\nPASS: 4 peers identical after 900 ticks\n", 0);
    CHECK(m.passed && m.line == "PASS: 4 peers identical after 900 ticks");
    m = parseMpTestOutput("FAIL: see build\\mp_test\n", 1);
    CHECK(!m.passed && m.line == "FAIL: see build\\mp_test");

    // Report → scenario: the recording copied next to it, inputs/ticks/scene lines, no checks (the engine's --update-expected adds them)
    const fs::path game = root / "game2";
    write(game / "bugreports" / "2026-11-02_10-00.zip",
          zip({{"info.txt", bytes("# MealKit bug report\nscene = scenes/arena.scene\nnote = stuck\ntick = 40\n")},
               {"replay.rec", recording(1, 3, 41)}}));
    write(game / "replays" / "2026-11-02_11-00.rec", recording(2, 4, 90));
    const std::vector<Entry> entries = listEntries(str(game));
    CHECK(entries.size() == 2);
    for (const Entry& e : entries) {
        Report report;
        CHECK(loadReport(e, report));
        std::string path, error;
        CHECK(writeScenarioFromReport(report, sample, path, error));
        std::string text;
        CHECK(mkcontract::readFile(path, text));
        const std::string name = std::string(kindName(e.kind)) + "_" + e.name;
        CHECK(fs::path(path).filename() == name + ".scenario");
        CHECK(text.find("inputs " + name + ".rec\n") != std::string::npos);
        CHECK(text.find("\ntick ") == std::string::npos);
        if (e.kind == Kind::Bug) CHECK(text.find("ticks 41\n") != std::string::npos && text.find("scene scenes/arena.scene\n") != std::string::npos);
        else CHECK(text.find("ticks ") == std::string::npos);
        std::vector<uint8_t> rec;
        std::ifstream in(fs::path(path).replace_extension(".rec"), std::ios::binary);
        rec.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        CHECK(readRecording(rec).valid);
        CHECK(!writeScenarioFromReport(report, sample, path, error)); // already there
    }
    Report empty;
    std::string path, error;
    CHECK(!writeScenarioFromReport(empty, sample, path, error));
}

// Whether all tr("...")/trf("...") text in tool/App.cpp is in the Korean table
void testTranslations() {
    std::ifstream in(fs::path(MKQA_SOURCE_DIR) / "tool" / "App.cpp", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!text.empty());
    setLanguage("ko");
    int count = 0;
    for (size_t pos = 0; (pos = text.find("tr", pos)) != std::string::npos; pos += 2) {
        size_t open = pos + 2;
        if (open < text.size() && text[open] == 'f') ++open;
        if (text.compare(open, 2, "(\"") != 0 || (pos > 0 && (std::isalnum(static_cast<unsigned char>(text[pos - 1])) || text[pos - 1] == '_')))
            continue;
        std::string s;
        for (size_t i = open + 2; i < text.size() && text[i] != '"'; ++i) {
            if (text[i] == '\\' && i + 1 < text.size()) ++i;
            s += text[i];
        }
        ++count;
        if (std::string(tr(s.c_str())) == s) {
            std::printf("  FAIL no Korean for \"%s\"\n", s.c_str());
            ++g_failures;
        }
    }
    CHECK(count > 20);
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "mkqa_tests";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    testRecording();
    testZip(root);
    testReports(root);
    testProject(root);
    testTests(root);
    testTranslations();
    fs::remove_all(root, ec);
    if (g_failures) {
        std::printf("FAIL (%d)\n", g_failures);
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}
