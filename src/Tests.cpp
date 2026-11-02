#include "mkqa/Tests.h"

#include <mkcontract/Files.h>
#include <mkcontract/Text.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace mkqa {
namespace {

#if defined(_WIN32)
constexpr const char* kExe = ".exe";
#else
constexpr const char* kExe = "";
#endif

std::string projectName(const Project& project) { return project.name.empty() ? std::string("MealKitGame") : project.name; }

std::string releaseBin(const Project& project, const std::string& file) {
    const std::string path = mkcontract::utf8Of(mkcontract::pathOf(buildRoot(project)) / "build" / "Release" / "bin" / file);
    return mkcontract::pathExists(path) ? path : std::string();
}

bool startsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

} // namespace

std::string buildRoot(const Project& project) {
    const fs::path dir = mkcontract::pathOf(project.directory);
    if (mkcontract::pathExists(mkcontract::utf8Of(dir / "build.bat"))) return project.directory;
    const fs::path samples = dir.parent_path();
    const fs::path engine = samples.parent_path();
    if (samples.filename() == "samples" && mkcontract::pathExists(mkcontract::utf8Of(engine / "build.bat")) &&
        mkcontract::pathExists(mkcontract::utf8Of(engine / "engine" / "src" / "mealkit")))
        return mkcontract::utf8Of(engine);
    return project.directory;
}

std::string headlessExe(const Project& project) { return releaseBin(project, projectName(project) + "_headless" + kExe); }
std::string gameExe(const Project& project) { return releaseBin(project, projectName(project) + kExe); }

std::vector<std::string> listScenarios(const Project& project) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const fs::directory_entry& e : fs::directory_iterator(mkcontract::pathOf(project.directory) / "tests", ec))
        if (e.is_regular_file() && e.path().extension() == ".scenario") out.push_back(mkcontract::utf8Of(e.path()));
    std::sort(out.begin(), out.end());
    return out;
}

ScenarioResult parseScenarioOutput(const std::string& output, int exitCode) {
    ScenarioResult r;
    r.output = output;
    bool summary = false;
    for (const std::string& line : mkcontract::splitLines(output)) {
        if (startsWith(line, "SCENARIO ok ")) {
            summary = true;
            const size_t at = line.rfind(" checks=");
            if (at != std::string::npos) r.checks = static_cast<uint32_t>(std::strtoul(line.c_str() + at + 8, nullptr, 10));
        } else if (startsWith(line, "SCENARIO FAIL ")) {
            const size_t at = line.rfind(" failed=");
            if (at != std::string::npos && line.find('/', at) != std::string::npos) { // summary "failed=K/N"
                summary = true;
                r.failed = static_cast<uint32_t>(std::strtoul(line.c_str() + at + 8, nullptr, 10));
                r.checks = static_cast<uint32_t>(std::strtoul(line.c_str() + line.find('/', at) + 1, nullptr, 10));
            } else {
                r.failures.push_back(line.substr(14));
            }
        }
    }
    r.state = summary && exitCode == 0 && r.failed == 0 && r.failures.empty() ? ScenarioResult::State::Passed
                                                                               : ScenarioResult::State::Failed;
    if (!summary) r.failures.push_back("no SCENARIO line (exit code " + std::to_string(exitCode) + ")");
    return r;
}

std::vector<std::string> scenarioArgs(const std::string& headless, const std::string& scenario, bool updateExpected) {
    std::vector<std::string> args = {headless, "--test", scenario};
    if (updateExpected) args.push_back("--update-expected");
    return args;
}

bool writeScenarioFromReport(const Report& report, const Project& project, std::string& scenarioPath, std::string& error) {
    if (!report.recording.valid) {
        error = "the report has no input recording";
        return false;
    }
    std::vector<uint8_t> bytes;
    if (report.entry.kind == Kind::Replay) {
        std::ifstream in(mkcontract::pathOf(report.entry.path), std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    } else {
        std::vector<ZipEntry> entries;
        if (!readZip(report.entry.path, entries, error)) return false;
        for (ZipEntry& e : entries)
            if (e.name == "replay.rec") bytes = std::move(e.data);
    }
    if (bytes.empty()) {
        error = "cannot read the recording";
        return false;
    }
    const fs::path dir = mkcontract::pathOf(project.directory) / "tests";
    const std::string name = std::string(kindName(report.entry.kind)) + "_" + report.entry.name;
    scenarioPath = mkcontract::utf8Of(dir / (name + ".scenario"));
    const std::string recPath = mkcontract::utf8Of(dir / (name + ".rec"));
    if (mkcontract::pathExists(scenarioPath) || mkcontract::pathExists(recPath)) {
        error = "tests/" + name + ".scenario already exists";
        return false;
    }
    std::error_code ec;
    fs::create_directories(dir, ec);
    {
        std::ofstream out(mkcontract::pathOf(recPath), std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            error = "cannot write " + recPath;
            return false;
        }
    }
    std::string text = "# From MealKit QA: " + std::string(kindName(report.entry.kind)) + " " + report.entry.name;
    if (!report.value("note").empty()) text += " \"" + report.value("note") + "\"";
    text += "\n# The checks below are every state value at the report's tick. Keep the ones that state the bug (fix it, then\n"
            "# make them say what should happen) and delete the rest.\n";
    text += "inputs " + name + ".rec\n";
    const std::string tick = report.value("tick");
    if (!tick.empty()) text += "ticks " + std::to_string(std::strtoul(tick.c_str(), nullptr, 10) + 1) + "\n"; // replays: all of it
    if (!report.value("scene").empty()) text += "scene " + report.value("scene") + "\n";
    if (!mkcontract::writeFile(scenarioPath, text)) {
        error = "cannot write " + scenarioPath;
        return false;
    }
    return true;
}

MpTestResult parseMpTestOutput(const std::string& output, int exitCode) {
    MpTestResult r;
    r.done = true;
    r.output = output;
    for (const std::string& line : mkcontract::splitLines(output))
        if (startsWith(line, "PASS:") || startsWith(line, "FAIL:")) r.line = line;
    r.passed = exitCode == 0 && startsWith(r.line, "PASS:");
    if (r.line.empty()) r.line = "FAIL: no result line (exit code " + std::to_string(exitCode) + ")";
    return r;
}

std::vector<std::string> mpTestArgs(const Project& project, uint32_t clients, uint32_t ticks) {
    // A reference sample has no engine line (its engine is the build root)
    std::string script = mkcontract::utf8Of(mkcontract::pathOf(project.engine) / "tools" / "mp_test.ps1");
    if (!mkcontract::pathExists(script)) script = mkcontract::utf8Of(mkcontract::pathOf(buildRoot(project)) / "tools" / "mp_test.ps1");
#if defined(_WIN32)
    std::vector<std::string> args = {"powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script};
#else
    std::vector<std::string> args = {"pwsh", "-NoProfile", "-File", script};
#endif
    for (const std::string& a : {std::string("-Exe"), gameExe(project), std::string("-Headless"), std::string("-Clients"),
                                 std::to_string(clients), std::string("-Ticks"), std::to_string(ticks)})
        args.push_back(a);
    return args;
}

} // namespace mkqa
