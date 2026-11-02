#pragma once

// MealKit QA screen: the game's reports and replays on the left, the chosen one on the right. Window/fonts/loop are in main.cpp.
#include "Runner.h"
#include "mkqa/Reports.h"
#include "mkqa/Tests.h"

#include <map>
#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

namespace mkqa::tool {

class App {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    // project: --project folder (else the most recent project); select: file name to select (--select)
    void init(const std::string& project, const std::string& select);
    void showTests() { m_selectTests = true; } // --tests
    void frame(double dt);
    void shutdown();
    std::string title() const;

private:
    void useProject(const std::string& directory);
    void refresh();
    void select(int index);
    void drawTop();
    void drawList();
    void drawDetail();
    void drawDeletePopup();
    void openInEditor(const Report& report);
    // Tests page: the project's scenario tests and mp_test (tool/Runner.h runs them one at a time)
    void drawTests();
    void drawTestDetail();
    void refreshScenarios();
    void runScenario(const std::string& file, bool updateExpected);
    void runMpTest();
    void saveAsScenario(const Report& report);
    void pollJobs();
    bool needBuild(const std::string& exe); // exe "" → status message, true
    void setStatus(const std::string& text, bool error);

    std::vector<Project> m_projects; // the current one first, then recent projects
    Project m_project;
    std::string m_folder; // game folder of m_project
    std::vector<Report> m_reports;
    std::string m_listKey; // file names + sizes at the last refresh (rescanned when it changes)
    double m_sinceScan = 0.0;
    int m_selected = -1;
    std::string m_selectName;

    bool m_showBug = true, m_showCrash = true, m_showReplay = true;
    char m_search[128] = {};
    char m_logFilter[128] = {};

    SDL_Texture* m_shot = nullptr;
    float m_shotW = 0, m_shotH = 0;
    bool m_confirmDelete = false;

    bool m_testsPage = false;
    bool m_selectTests = false;                      // switch to the Tests page next frame
    std::vector<std::string> m_scenarios;            // full paths
    std::map<std::string, ScenarioResult> m_results; // by path
    std::map<int, std::string> m_scenarioJobs;       // job id → scenario path
    int m_mpJob = 0;
    MpTestResult m_mp;
    int m_mpClients = 3, m_mpTicks = 900;
    std::string m_testSelected; // scenario path, "mp_test", or ""
    Runner m_runner;

    std::string m_status;
    bool m_statusError = false;
};

} // namespace mkqa::tool
