#include "App.h"

#include "Strings.h"

#include <mkcontract/Files.h>
#include <mkcontract/User.h>

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace mkqa::tool {
namespace {

constexpr double kScanSeconds = 2.0; // new reports appear while the game runs

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

bool contains(const std::string& text, const char* query) { return !*query || lower(text).find(lower(query)) != std::string::npos; }

const char* kindLabel(Kind kind) {
    switch (kind) {
    case Kind::Bug: return tr("Bug report");
    case Kind::Crash: return tr("Crash");
    case Kind::Replay: return tr("Replay");
    }
    return "?";
}

ImVec4 kindColor(Kind kind) {
    switch (kind) {
    case Kind::Bug: return ImVec4(0.95f, 0.75f, 0.30f, 1.0f);
    case Kind::Crash: return ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
    case Kind::Replay: return ImVec4(0.55f, 0.75f, 0.95f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

std::string listKey(const std::vector<Entry>& entries) {
    std::string key;
    for (const Entry& e : entries) key += e.path + "|" + std::to_string(e.bytes) + "\n";
    return key;
}

std::string sizeText(uint64_t bytes) {
    char s[32];
    if (bytes >= 1024 * 1024) std::snprintf(s, sizeof(s), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else std::snprintf(s, sizeof(s), "%.0f KB", static_cast<double>(bytes) / 1024.0);
    return s;
}

std::string folderOf(const std::string& path) { return mkcontract::utf8Of(mkcontract::pathOf(path).parent_path()); }

} // namespace

void App::init(const std::string& project, const std::string& select) {
    setLanguage(mkcontract::editorSetting("language").empty() ? "ko" : mkcontract::editorSetting("language"));
    m_selectName = select;
    std::vector<std::string> folders;
    if (!project.empty()) folders.push_back(project);
    for (const std::string& f : mkcontract::recentProjects()) folders.push_back(f);
    for (const std::string& f : folders) {
        Project p;
        if (!readProject(f, p)) continue;
        bool seen = false;
        for (const Project& q : m_projects) seen = seen || mkcontract::sameDirectory(q.directory, p.directory);
        if (!seen) m_projects.push_back(p);
    }
    if (!m_projects.empty()) useProject(m_projects[0].directory);
    else if (!project.empty()) setStatus(trf("%s has no mealkit.project", project.c_str()), true);
}

void App::shutdown() {
    if (m_shot) SDL_DestroyTexture(m_shot);
    m_shot = nullptr;
}

std::string App::title() const {
    return m_project.valid ? "MealKit QA - " + (m_project.name.empty() ? std::string("MealKitGame") : m_project.name) : "MealKit QA";
}

void App::useProject(const std::string& directory) {
    for (const Project& p : m_projects)
        if (mkcontract::sameDirectory(p.directory, directory)) m_project = p;
    m_folder = gameFolder(m_project.name);
    m_listKey.clear();
    m_reports.clear();
    select(-1);
    refresh();
}

void App::refresh() {
    m_sinceScan = 0.0;
    const std::vector<Entry> entries = listEntries(m_folder);
    const std::string key = listKey(entries);
    if (key == m_listKey) return;
    m_listKey = key;
    const std::string selectedPath = m_selected >= 0 ? m_reports[static_cast<size_t>(m_selected)].entry.path : std::string();
    m_reports.clear();
    for (const Entry& e : entries) {
        Report r;
        loadReport(e, r); // a broken one stays in the list with its error
        m_reports.push_back(std::move(r));
    }
    int index = -1;
    for (size_t i = 0; i < m_reports.size(); ++i) {
        const Entry& e = m_reports[i].entry;
        if ((!selectedPath.empty() && e.path == selectedPath) || (!m_selectName.empty() && e.name == m_selectName)) index = static_cast<int>(i);
    }
    m_selectName.clear();
    if (index < 0 && !m_reports.empty()) index = 0;
    m_selected = -1; // reload the screenshot
    select(index);
}

void App::select(int index) {
    if (index == m_selected) return;
    m_selected = index;
    if (m_shot) SDL_DestroyTexture(m_shot);
    m_shot = nullptr;
    m_logFilter[0] = 0;
    if (index < 0 || !renderer) return;
    const Report& r = m_reports[static_cast<size_t>(index)];
    if (r.screenshot.empty()) return;
    SDL_IOStream* io = SDL_IOFromConstMem(r.screenshot.data(), r.screenshot.size());
    if (SDL_Surface* surface = io ? SDL_LoadPNG_IO(io, true) : nullptr) {
        m_shot = SDL_CreateTextureFromSurface(renderer, surface);
        m_shotW = static_cast<float>(surface->w);
        m_shotH = static_cast<float>(surface->h);
        SDL_DestroySurface(surface);
    }
}

void App::setStatus(const std::string& text, bool error) {
    m_status = text;
    m_statusError = error;
}

void App::frame(double dt) {
    m_sinceScan += dt;
    if (m_sinceScan >= kScanSeconds && m_project.valid) refresh();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("##qa", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    drawTop();
    ImGui::Separator();
    const float statusHeight = ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginTable("##split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV,
                          ImVec2(0, ImGui::GetContentRegionAvail().y - statusHeight))) {
        ImGui::TableSetupColumn("list", ImGuiTableColumnFlags_WidthStretch, 0.38f);
        ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthStretch, 0.62f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawList();
        ImGui::TableNextColumn();
        drawDetail();
        ImGui::EndTable();
    }
    ImGui::Separator();
    if (m_status.empty()) ImGui::TextDisabled("%s", m_folder.c_str());
    else if (m_statusError) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", m_status.c_str());
    else ImGui::TextUnformatted(m_status.c_str());
    drawDeletePopup();
    ImGui::End();
}

void App::drawTop() {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("Game"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 18);
    const std::string current = m_project.valid ? (m_project.name.empty() ? std::string("MealKitGame") : m_project.name) : tr("(no project)");
    if (ImGui::BeginCombo("##game", current.c_str())) {
        for (const Project& p : m_projects) {
            const std::string label = (p.name.empty() ? std::string("MealKitGame") : p.name) + "  " + p.directory;
            if (ImGui::Selectable(label.c_str(), mkcontract::sameDirectory(p.directory, m_project.directory))) useProject(p.directory);
        }
        if (m_projects.empty()) ImGui::TextDisabled("%s", tr("No recent projects. Open a project in the editor first."));
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Refresh"))) {
        m_listKey.clear();
        refresh();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Open Folder")) && !m_folder.empty()) SDL_OpenURL(("file:///" + m_folder).c_str());
    ImGui::SameLine();
    ImGui::Checkbox(tr("Bug reports"), &m_showBug);
    ImGui::SameLine();
    ImGui::Checkbox(tr("Crashes"), &m_showCrash);
    ImGui::SameLine();
    ImGui::Checkbox(tr("Replays"), &m_showReplay);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", tr("Search note, scene, version"), m_search, sizeof(m_search));
}

void App::drawList() {
    if (!m_project.valid) {
        ImGui::TextWrapped("%s", tr("Pick a game. MealKit QA lists the projects you opened in the editor."));
        return;
    }
    if (m_reports.empty()) {
        ImGui::TextWrapped("%s", tr("No reports yet. Press F9 in the game to save a bug report; crashes and finished matches show up here too."));
        return;
    }
    if (!ImGui::BeginTable("##reports", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH)) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(tr("When"), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 8.5f);
    ImGui::TableSetupColumn(tr("Kind"), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.5f);
    ImGui::TableSetupColumn(tr("Note"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (size_t i = 0; i < m_reports.size(); ++i) {
        const Report& r = m_reports[i];
        const Kind kind = r.entry.kind;
        if ((kind == Kind::Bug && !m_showBug) || (kind == Kind::Crash && !m_showCrash) || (kind == Kind::Replay && !m_showReplay)) continue;
        const std::string haystack = r.entry.name + " " + r.value("note") + " " + r.value("scene") + " " + r.value("gameVersion") + " " +
                                     r.value("engine") + " " + r.value("commit");
        if (!contains(haystack, m_search)) continue;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(static_cast<int>(i));
        const std::string when = r.entry.stamp.empty() ? r.entry.name : r.entry.stamp;
        if (ImGui::Selectable(when.c_str(), m_selected == static_cast<int>(i), ImGuiSelectableFlags_SpanAllColumns))
            select(static_cast<int>(i));
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) openInEditor(r);
        ImGui::PopID();
        ImGui::TableNextColumn();
        ImGui::TextColored(kindColor(kind), "%s", kindLabel(kind));
        ImGui::TableNextColumn();
        if (!r.error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", r.error.c_str());
        else if (kind == Kind::Replay)
            ImGui::TextDisabled("%s", trf("%u player(s), %u s", r.recording.players, r.recording.ticks / std::max(1u, m_project.tickRate)).c_str());
        else if (!r.value("note").empty()) ImGui::TextUnformatted(r.value("note").c_str());
        else ImGui::TextDisabled("%s", trf("tick %s", r.value("tick").c_str()).c_str());
    }
    ImGui::EndTable();
}

void App::drawDetail() {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_reports.size())) return;
    const Report& r = m_reports[static_cast<size_t>(m_selected)];
    ImGui::TextColored(kindColor(r.entry.kind), "%s", kindLabel(r.entry.kind));
    ImGui::SameLine();
    ImGui::TextUnformatted((r.entry.stamp.empty() ? r.entry.name : r.entry.stamp).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", sizeText(r.entry.bytes).c_str());

    const bool playable = r.recording.valid;
    ImGui::BeginDisabled(!playable);
    if (ImGui::Button(tr("Open in Editor"))) openInEditor(r);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", playable ? tr("Plays the recording in the editor and pauses where the report was made")
                                         : tr("This report has no input recording (made outside a match)"));
    ImGui::SameLine();
    if (ImGui::Button(tr("Copy Info"))) {
        SDL_SetClipboardText(r.summaryText().c_str());
        setStatus(tr("Copied the report info. Paste it into the issue."), false);
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Show File"))) SDL_OpenURL(("file:///" + folderOf(r.entry.path)).c_str());
    ImGui::SameLine();
    if (ImGui::Button(tr("Delete"))) m_confirmDelete = true;

    if (!r.error.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", trf("Cannot read: %s", r.error.c_str()).c_str());
        return;
    }
    if (!r.value("note").empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("\"%s\"", r.value("note").c_str());
    }
    ImGui::Spacing();
    if (ImGui::BeginTable("##info", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
        const auto row = [](const char* key, const std::string& value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", key);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(value.c_str());
        };
        for (const auto& [k, v] : r.info)
            if (k != "note") row(k.c_str(), v);
        if (r.recording.valid) {
            char seed[32];
            std::snprintf(seed, sizeof(seed), "%llu", static_cast<unsigned long long>(r.recording.seed));
            row(tr("recording"), trf("%u player(s), %u tick(s) (%.1f s), seed %s", r.recording.players, r.recording.ticks,
                                    static_cast<double>(r.recording.ticks) / std::max(1u, m_project.tickRate), seed));
        } else if (r.entry.kind != Kind::Replay) {
            row(tr("recording"), tr("none (made outside a match)"));
        }
        if (r.hasDump) row("crash.dmp", sizeText(r.dumpBytes));
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (!ImGui::BeginTabBar("##tabs")) return;
    if (m_shot && ImGui::BeginTabItem(tr("Screenshot"))) {
        const float w = ImGui::GetContentRegionAvail().x;
        const float scale = std::min(1.0f, w / std::max(1.0f, m_shotW));
        ImGui::Image(reinterpret_cast<ImTextureID>(m_shot), ImVec2(m_shotW * scale, m_shotH * scale));
        ImGui::EndTabItem();
    }
    if (!r.crash.empty() && ImGui::BeginTabItem(tr("Crash"))) {
        ImGui::InputTextMultiline("##crash", const_cast<char*>(r.crash.c_str()), r.crash.size() + 1, ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
        ImGui::EndTabItem();
    }
    if (!r.log.empty() && ImGui::BeginTabItem(tr("Log"))) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##logfilter", tr("Filter lines (e.g. WARN, ERROR)"), m_logFilter, sizeof(m_logFilter));
        if (ImGui::BeginChild("##log", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
            size_t start = 0;
            while (start < r.log.size()) {
                size_t end = r.log.find('\n', start);
                if (end == std::string::npos) end = r.log.size();
                const std::string line = r.log.substr(start, end - start);
                start = end + 1;
                if (!contains(line, m_logFilter)) continue;
                if (line.find("ERROR") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", line.c_str());
                else if (line.find("WARN") != std::string::npos) ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "%s", line.c_str());
                else ImGui::TextUnformatted(line.c_str());
            }
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

void App::drawDeletePopup() {
    if (m_confirmDelete) {
        ImGui::OpenPopup("##delete");
        m_confirmDelete = false;
    }
    if (!ImGui::BeginPopupModal("##delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) return;
    if (m_selected < 0) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const Entry entry = m_reports[static_cast<size_t>(m_selected)].entry;
    ImGui::TextUnformatted(trf("Delete %s? This can't be undone.", entry.name.c_str()).c_str());
    if (ImGui::Button(tr("Delete"))) {
        std::string error;
        if (removeEntry(entry, error)) setStatus(trf("Deleted %s", entry.name.c_str()), false);
        else setStatus(trf("Cannot delete %s: %s", entry.name.c_str(), error.c_str()), true);
        refresh();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel")) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void App::openInEditor(const Report& report) {
    if (!report.recording.valid) return;
    const std::string editor = editorExe(m_project.engine);
    if (editor.empty()) {
        setStatus(trf("The editor isn't built in %s (build\\Editor\\bin)", m_project.engine.c_str()), true);
        return;
    }
    const std::vector<std::string> args = editorReplayArgs(editor, m_project, report.entry.path);
    std::vector<const char*> argv;
    for (const std::string& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    SDL_Process* process = SDL_CreateProcess(argv.data(), false);
    if (!process) {
        setStatus(trf("Cannot start the editor: %s", SDL_GetError()), true);
        return;
    }
    SDL_DestroyProcess(process); // keeps running
    setStatus(trf("Opened %s in the editor", report.entry.name.c_str()), false);
}

} // namespace mkqa::tool
