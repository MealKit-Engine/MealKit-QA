// MealKit QA executable: SDL3 window + SDL_Renderer + Dear ImGui (same framework as MealKit Audio/Manager). The actual screen is App.
//   MealKitQA [--project <MealKit project folder>] [--select <report name>] [--lang ko|en] [--tests] [--frames N --capture <png>]
//   MealKitQA --list [--project <folder>]   prints the game's reports and replays without a window (for tests/scripts)
//   MealKitQA --run-tests [--project <folder>] [--mp-test]   runs tests/*.scenario (and mp_test) without a window, exit code 1 on a failure
//   MealKitQA --make-scenario <report name> [--project <folder>]   tests/<kind>_<name>.scenario from a report's recording, checks filled
//   MealKitQA --version
#include "App.h"
#include "Strings.h"

#include <mkcontract/User.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Merges a system font + a Korean font (ImGui's default vector font if none)
void loadFonts(float size) {
    ImGuiIO& io = ImGui::GetIO();
    const char* latin[] = {
#if defined(_WIN32)
        "C:\\Windows\\Fonts\\segoeui.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/Supplemental/Arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
#endif
    };
    const char* korean[] = {
#if defined(_WIN32)
        "C:\\Windows\\Fonts\\malgun.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/AppleSDGothicNeo.ttc",
#else
        "/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
#endif
    };
    bool loaded = false;
    for (const char* path : latin)
        if (!loaded && SDL_GetPathInfo(path, nullptr) && io.Fonts->AddFontFromFileTTF(path, size)) loaded = true;
    if (!loaded) {
        ImFontConfig config;
        config.SizePixels = size;
        io.Fonts->AddFontDefaultVector(&config);
    }
    ImFontConfig merge;
    merge.MergeMode = true;
    for (const char* path : korean)
        if (SDL_GetPathInfo(path, nullptr) && io.Fonts->AddFontFromFileTTF(path, size, &merge)) break;
}

bool openProject(const std::string& projectArg, mkqa::Project& project) {
    std::string directory = projectArg;
    if (directory.empty()) {
        const std::vector<std::string> recent = mkcontract::recentProjects();
        if (!recent.empty()) directory = recent[0];
    }
    if (mkqa::readProject(directory, project)) return true;
    std::printf("No project (use --project <folder>)\n");
    return false;
}

// One command line, waiting for it
mkqa::tool::Runner::Job runNow(std::vector<std::string> args) {
    mkqa::tool::Runner runner;
    runner.add(std::move(args));
    for (;;) {
        std::vector<mkqa::tool::Runner::Job> done = runner.takeFinished();
        if (!done.empty()) return done[0];
        SDL_Delay(20);
    }
}

int runTests(const std::string& projectArg, bool mpTest) {
    mkqa::Project project;
    if (!openProject(projectArg, project)) return 1;
    const std::string headless = mkqa::headlessExe(project);
    if (headless.empty()) {
        std::printf("Build the game first: no %s_headless in %s/build/Release/bin\n", project.name.c_str(), mkqa::buildRoot(project).c_str());
        return 1;
    }
    int failed = 0;
    for (const std::string& file : mkqa::listScenarios(project)) {
        const mkqa::tool::Runner::Job job = runNow(mkqa::scenarioArgs(headless, file, false));
        const mkqa::ScenarioResult r = mkqa::parseScenarioOutput(job.output, job.exitCode);
        const bool ok = r.state == mkqa::ScenarioResult::State::Passed;
        failed += !ok;
        std::printf("%s %s checks=%u\n", ok ? "ok  " : "FAIL", file.c_str(), r.checks);
        for (const std::string& line : r.failures) std::printf("     %s\n", line.c_str());
    }
    if (mpTest) {
        const mkqa::tool::Runner::Job job = runNow(mkqa::mpTestArgs(project, 3, 900));
        const mkqa::MpTestResult r = mkqa::parseMpTestOutput(job.output, job.exitCode);
        failed += !r.passed;
        std::printf("mp_test %s\n", r.line.c_str());
    }
    std::fflush(stdout);
    return failed ? 1 : 0;
}

int makeScenario(const std::string& projectArg, const std::string& name) {
    mkqa::Project project;
    if (!openProject(projectArg, project)) return 1;
    const std::string headless = mkqa::headlessExe(project);
    for (const mkqa::Entry& e : mkqa::listEntries(mkqa::gameFolder(project.name))) {
        if (e.name != name) continue;
        mkqa::Report report;
        mkqa::loadReport(e, report);
        std::string path, error;
        if (headless.empty()) error = "the game's headless executable isn't built";
        else if (mkqa::writeScenarioFromReport(report, project, path, error)) {
            const mkqa::tool::Runner::Job job = runNow(mkqa::scenarioArgs(headless, path, true));
            const mkqa::ScenarioResult r = mkqa::parseScenarioOutput(job.output, job.exitCode);
            std::printf("%s %s checks=%u\n", r.state == mkqa::ScenarioResult::State::Passed ? "saved" : "FAIL", path.c_str(), r.checks);
            return r.state == mkqa::ScenarioResult::State::Passed ? 0 : 1;
        }
        std::printf("Cannot save the scenario: %s\n", error.c_str());
        return 1;
    }
    std::printf("No report named %s\n", name.c_str());
    return 1;
}

// --list: the project's (else the most recent project's) game folder
int printList(const std::string& projectArg) {
    std::string directory = projectArg;
    if (directory.empty()) {
        const std::vector<std::string> recent = mkcontract::recentProjects();
        if (!recent.empty()) directory = recent[0];
    }
    mkqa::Project project;
    if (!mkqa::readProject(directory, project)) {
        std::printf("No project (use --project <folder>)\n");
        return 1;
    }
    const std::string folder = mkqa::gameFolder(project.name);
    std::printf("game: %s %s\n", project.name.c_str(), folder.c_str());
    for (const mkqa::Entry& e : mkqa::listEntries(folder)) {
        mkqa::Report r;
        mkqa::loadReport(e, r);
        std::printf("%s %s", mkqa::kindName(e.kind), e.name.c_str());
        if (!r.error.empty()) std::printf(" error: %s", r.error.c_str());
        if (r.recording.valid) std::printf(" players %u ticks %u", r.recording.players, r.recording.ticks);
        if (!r.value("tick").empty()) std::printf(" tick %s", r.value("tick").c_str());
        if (!r.value("note").empty()) std::printf(" note \"%s\"", r.value("note").c_str());
        std::printf("\n");
    }
    std::fflush(stdout);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string project, capture, select, language;
    long frames = 0; // --frames N: quit after N frames (--capture saves the last screen as PNG, for docs/tests)
    bool list = false, runTestsArg = false, mpTest = false, testsPage = false;
    std::string makeScenarioArg;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc) project = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames = std::strtol(argv[++i], nullptr, 10);
        else if (std::strcmp(argv[i], "--capture") == 0 && i + 1 < argc) capture = argv[++i];
        else if (std::strcmp(argv[i], "--select") == 0 && i + 1 < argc) select = argv[++i];
        else if (std::strcmp(argv[i], "--lang") == 0 && i + 1 < argc) language = argv[++i];
        else if (std::strcmp(argv[i], "--list") == 0) list = true;
        else if (std::strcmp(argv[i], "--run-tests") == 0) runTestsArg = true;
        else if (std::strcmp(argv[i], "--tests") == 0) testsPage = true;
        else if (std::strcmp(argv[i], "--mp-test") == 0) mpTest = true;
        else if (std::strcmp(argv[i], "--make-scenario") == 0 && i + 1 < argc) makeScenarioArg = argv[++i];
        else if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("MealKit QA %s\n", MKQA_VERSION);
            return 0;
        }
    }
    if (list) return printList(project);
    if (runTestsArg) return runTests(project, mpTest);
    if (!makeScenarioArg.empty()) return makeScenario(project, makeScenarioArg);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    const float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_Window* window = SDL_CreateWindow("MealKit QA", static_cast<int>(1300 * scale), static_cast<int>(820 * scale),
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        SDL_Log("Cannot create the window: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Window layout goes in the user folder
    static std::string ini;
    if (char* pref = SDL_GetPrefPath("MealKit", "MealKit QA")) {
        ini = std::string(pref) + "imgui.ini";
        SDL_free(pref);
        io.IniFilename = ini.c_str();
    }
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    style.FrameRounding = 3.0f;
    loadFonts(16.0f);
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    mkqa::tool::App app;
    app.window = window;
    app.renderer = renderer;
    app.init(project, select);
    if (testsPage) app.showTests();
    if (!language.empty()) mkqa::setLanguage(language);

    Uint64 last = SDL_GetPerformanceCounter();
    std::string shownTitle;
    long frame = 0;
    for (bool running = true; running;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) running = false;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }
        const Uint64 now = SDL_GetPerformanceCounter();
        const double dt = static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceFrequency());
        last = now;

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        app.frame(dt);
        ImGui::Render();

        const std::string title = app.title();
        if (title != shownTitle) {
            SDL_SetWindowTitle(window, title.c_str());
            shownTitle = title;
        }
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, 0.08f, 0.08f, 0.09f, 1.0f);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        if (frames > 0 && ++frame >= frames) {
            if (!capture.empty()) {
                SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
                if (!shot || !SDL_SavePNG(shot, capture.c_str())) SDL_Log("Cannot save %s: %s", capture.c_str(), SDL_GetError());
                SDL_DestroySurface(shot);
            }
            running = false;
        }
        SDL_RenderPresent(renderer);
    }

    app.shutdown();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
