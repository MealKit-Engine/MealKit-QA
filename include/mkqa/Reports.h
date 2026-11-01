#pragma once

// What MealKit QA reads: a game's bug reports, crash reports and replays in the per-game user folder. No window/ImGui (tests/QaTests.cpp).
// Connects to the engine only through files (engine docs: "Bug report bundles", "Crash reports", "Saving replays"):
//   <game folder>/bugreports/<YYYY-MM-DD_HH-MM[-n]>.zip   info.txt, log.txt, screenshot.png, replay.rec (absent outside a match)
//   <game folder>/crash/<YYYY-MM-DD_HH-MM[-n]>.zip        info.txt, crash.txt, log.txt, crash.dmp, replay.rec
//   <game folder>/replays/<YYYY-MM-DD_HH-MM[-n]>.rec      input recording "MKINPUT1"
//   <game folder> = <user data folder>/<name line of mealkit.project>  (Windows %LOCALAPPDATA%)
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mkqa {

// Stored (uncompressed) zip files like the engine writes. Compressed entries are refused (a zip repacked by another program)
struct ZipEntry {
    std::string name;
    std::vector<uint8_t> data;
};
bool readZip(const std::string& path, std::vector<ZipEntry>& out, std::string& error);

// Header of an input recording (engine game/InputRecord.cpp, versions 1-3)
struct Recording {
    bool valid = false;
    uint32_t version = 0;
    uint32_t players = 0;
    uint64_t seed = 0;
    uint64_t contentHash = 0;
    uint32_t ticks = 0; // whole ticks in the file
    std::string error;
};
Recording readRecording(const std::vector<uint8_t>& bytes);

enum class Kind { Bug, Crash, Replay };
const char* kindName(Kind kind); // "bug", "crash", "replay"

// One file in the list
struct Entry {
    Kind kind = Kind::Bug;
    std::string path;
    std::string name;  // file name without extension ("2026-10-28_13-53-2")
    std::string stamp; // "2026-10-28 13:53" ("" if the name isn't a stamp)
    uint64_t bytes = 0;
};
// Every report and replay of the game folder, newest first (by name stamp, then -n)
std::vector<Entry> listEntries(const std::string& gameFolder);

struct Report {
    Entry entry;
    std::vector<std::pair<std::string, std::string>> info; // info.txt "key = value" lines in order
    std::string header;                                    // first # line of info.txt ("MealKit bug report")
    std::string log;
    std::string crash;                    // crash.txt
    std::vector<uint8_t> screenshot;      // PNG
    bool hasDump = false;                 // crash.dmp
    uint64_t dumpBytes = 0;
    Recording recording;                  // replay.rec, or the .rec file itself
    std::string error;                    // why it couldn't be read (the rest stays empty)

    std::string value(const std::string& key) const; // info value, "" if absent
    // Text for pasting into an issue tracker: file, header, info lines, crash.txt
    std::string summaryText() const;
};
bool loadReport(const Entry& entry, Report& out);

// mealkit.project of a project folder
struct Project {
    std::string directory;
    std::string name;     // "" → the engine uses "MealKitGame"
    std::string engine;   // engine line, else the project folder itself (the engine repo)
    uint32_t tickRate = 30;
    bool valid = false;
};
bool readProject(const std::string& directory, Project& out);

// The engine's per-game folder: <user data folder>/<name or MealKitGame> (Windows %LOCALAPPDATA%, macOS ~/Library/Application Support,
// other $XDG_DATA_HOME or ~/.local/share). MEALKIT_GAME_DIR replaces the whole path (tests)
std::string gameFolder(const std::string& name);
// Built editor of an engine folder (build/Editor/bin/MealKitEditor[.exe]), "" if not built
std::string editorExe(const std::string& engineDirectory);
// Command line that opens the report/replay in the editor (it plays to the report's tick and pauses)
std::vector<std::string> editorReplayArgs(const std::string& editor, const Project& project, const std::string& file);

bool removeEntry(const Entry& entry, std::string& error);

} // namespace mkqa
