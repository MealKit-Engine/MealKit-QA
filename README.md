# MealKit QA

A QA tool for games made with the [MealKit engine](https://github.com/MealKit-Engine/MealKit). It shows a game's bug reports, crash reports and replays in one list and opens any of them in the MealKit editor, which plays the recording and pauses where the report was made. It also runs the project's scenario tests and the engine's multiplayer test, and turns a report into a scenario test.

C++20, SDL3 + Dear ImGui (the same framework as MealKit Audio and MealKit Manager). MIT license.

## What it reads

The game writes these files into its per-game user folder (`%LOCALAPPDATA%\<project name>` on Windows). MealKit QA only reads them and does not link the engine.

| Folder | File | Made by |
|---|---|---|
| `bugreports/` | `YYYY-MM-DD_HH-MM.zip`: `info.txt`, `log.txt`, `screenshot.png`, `replay.rec` | F9 in the game |
| `crash/` | `YYYY-MM-DD_HH-MM.zip`: `info.txt`, `crash.txt`, `log.txt`, `crash.dmp`, `replay.rec` | the next run after a crash |
| `replays/` | `YYYY-MM-DD_HH-MM.rec` | every finished match (project setting `keepReplays`) |

## Screen

- **Game**: the project from `--project`, then the projects recently opened in the editor (the shared MealKit user folder, read with [mkcontract](https://github.com/MealKit-Engine/MealKit-Contract)).
- **List**: newest first, with filters for kind and a search over note, scene and versions. New reports appear while the game runs.
- **Detail**: note, `info.txt` values, recording length and seed, screenshot, crash text, log with a line filter.
- **Buttons**:
  - **Open in Editor**: runs `<engine>\build\Editor\bin\MealKitEditor.exe --project <project> --replay <file>`. Double-clicking a list row does the same.
  - **Copy Info**: copies the report info as text for an issue tracker.
  - **Show File**: opens the folder that holds the file.
  - **Save as Scenario**: copies the recording to the project's `tests/<kind>_<name>.rec` and writes `tests/<kind>_<name>.scenario` that runs to the report's tick. The game's headless build then fills in one `==` check per top-level state value (`--update-expected`); keep the checks that show the bug.
  - **Delete**: removes the file.
- **Tests page**: the project's `tests/*.scenario` with **Run All**, run one (double-click), **Update Expected**, the failed checks, the file and the output. **Run mp_test** runs the engine's `tools/mp_test.ps1` headless with the chosen clients and ticks. Both need the game's Release build (`build\Releasein\<name>_headless.exe`, `<name>.exe`; for the engine's reference samples, the engine's build folder).

## Build

```bat
build.bat            :: build\bin\MealKitQA.exe + tests
build.bat install    :: also copy to %LOCALAPPDATA%\MealKit\tools\qa (the editor's [Tools] menu finds it)
```

Other systems: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build`.

Command line: `MealKitQA [--project <folder>] [--select <report name>] [--tests] [--lang ko|en]`. Without a window: `--list` prints the reports, `--run-tests [--mp-test]` runs the scenarios (and mp_test) and exits with 1 on a failure, `--make-scenario <report name>` does Save as Scenario.

## Third-party code

| Folder | What | License |
|---|---|---|
| `third_party/SDL` | SDL 3 | zlib |
| `third_party/imgui` | Dear ImGui (docking) | MIT |
| `third_party/mkcontract` | MealKit tool contract library | MIT |
