#pragma once

// What MealKit QA runs: a project's scenario tests and the engine's mp_test. No window/ImGui (tests/QaTests.cpp); the tool starts the
// processes (tool/Runner.*). Connects to the engine only through command lines and their output (engine game/Scenario.h, tools/mp_test.ps1):
//   <game>_headless --test <project>/tests/<name>.scenario     → "SCENARIO FAIL <file>:<line> ..." lines, then "SCENARIO ok <file> checks=N"
//                                                                or "SCENARIO FAIL <file> failed=K/N" (exit code 6)
//   --update-expected on a file without checks writes one == check per top-level state value at the last tick
//   powershell -File <engine>/tools/mp_test.ps1 -Exe <game exe> -Headless -Clients N -Ticks T   → "PASS: ..." or "FAIL: ..."
#include "mkqa/Reports.h"

#include <string>
#include <vector>

namespace mkqa {

// Folder whose build/ holds the project's executables: the engine repo for its reference samples (samples/<name>), else the project
// (the engine's ProjectInfo::buildRoot)
std::string buildRoot(const Project& project);
// build/Release/bin/<name>_headless[.exe] and <name>[.exe] of the build root ("" if not built)
std::string headlessExe(const Project& project);
std::string gameExe(const Project& project);

// <project>/tests/*.scenario, by name
std::vector<std::string> listScenarios(const Project& project);

struct ScenarioResult {
    enum class State { NotRun, Running, Passed, Failed };
    State state = State::NotRun;
    uint32_t checks = 0;               // checks in the file (ok: all passed)
    uint32_t failed = 0;
    std::vector<std::string> failures; // "SCENARIO FAIL <file>:<line> ..." lines without the prefix
    std::string output;                // the whole output
};
// From the process output and exit code. No SCENARIO line (crash, missing scene) is a failure with the output as the reason
ScenarioResult parseScenarioOutput(const std::string& output, int exitCode);
std::vector<std::string> scenarioArgs(const std::string& headless, const std::string& scenario, bool updateExpected);

// Scenario test from a report's recording: tests/<kind>_<name>.rec + tests/<kind>_<name>.scenario (inputs, ticks to the report's tick,
// scene) without checks; running it with --update-expected then writes the checks. A file of that name is refused
bool writeScenarioFromReport(const Report& report, const Project& project, std::string& scenarioPath, std::string& error);

struct MpTestResult {
    bool done = false;
    bool passed = false;
    std::string line;   // the PASS/FAIL line
    std::string output;
};
MpTestResult parseMpTestOutput(const std::string& output, int exitCode);
std::vector<std::string> mpTestArgs(const Project& project, uint32_t clients, uint32_t ticks);

} // namespace mkqa
