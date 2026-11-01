#pragma once

// MealKit QA screen: the game's reports and replays on the left, the chosen one on the right. Window/fonts/loop are in main.cpp.
#include "mkqa/Reports.h"

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

    std::string m_status;
    bool m_statusError = false;
};

} // namespace mkqa::tool
