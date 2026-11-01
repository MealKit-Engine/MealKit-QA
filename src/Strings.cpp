#include "Strings.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace mkqa {
namespace {

struct Pair {
    const char* en;
    const char* ko;
};

// When new text is shown on screen, add its Korean here (tests/QaTests.cpp checks that all tr text in tool/App.cpp exists)
constexpr Pair kKorean[] = {
    {"Bug report", "버그 리포트"},
    {"Crash", "크래시"},
    {"Replay", "리플레이"},
    {"%s has no mealkit.project", "%s에 mealkit.project가 없다"},
    {"(no project)", "(프로젝트 없음)"},
    {"Game", "게임"},
    {"No recent projects. Open a project in the editor first.", "최근 프로젝트가 없습니다. 먼저 에디터에서 프로젝트를 여세요."},
    {"Refresh", "새로 고침"},
    {"Open Folder", "폴더 열기"},
    {"Bug reports", "버그 리포트"},
    {"Crashes", "크래시"},
    {"Replays", "리플레이"},
    {"Search note, scene, version", "메모, 장면, 버전 검색"},
    {"Pick a game. MealKit QA lists the projects you opened in the editor.", "게임을 고르세요. MealKit QA는 에디터에서 연 프로젝트를 보여 줍니다."},
    {"No reports yet. Press F9 in the game to save a bug report; crashes and finished matches show up here too.",
     "아직 리포트가 없습니다. 게임에서 F9를 누르면 버그 리포트가 저장되고, 크래시와 끝난 매치도 여기에 나옵니다."},
    {"When", "시각"},
    {"Kind", "종류"},
    {"Note", "메모"},
    {"%u player(s), %u s", "%u명, %u초"},
    {"tick %s", "틱 %s"},
    {"Open in Editor", "에디터에서 열기"},
    {"Plays the recording in the editor and pauses where the report was made", "에디터에서 녹화를 재생하고 리포트를 만든 지점에서 멈춥니다"},
    {"This report has no input recording (made outside a match)", "이 리포트에는 입력 녹화가 없습니다 (매치 밖에서 만듦)"},
    {"Copy Info", "정보 복사"},
    {"Copied the report info. Paste it into the issue.", "리포트 정보를 복사했다. 이슈에 붙여 넣으세요."},
    {"Show File", "파일 위치 열기"},
    {"Delete", "삭제"},
    {"Cannot read: %s", "읽을 수 없다: %s"},
    {"recording", "녹화"},
    {"%u player(s), %u tick(s) (%.1f s), seed %s", "%u명, %u틱 (%.1f초), 시드 %s"},
    {"none (made outside a match)", "없음 (매치 밖에서 만듦)"},
    {"Screenshot", "스크린샷"},
    {"Log", "로그"},
    {"Filter lines (e.g. WARN, ERROR)", "줄 거르기 (예: WARN, ERROR)"},
    {"Delete %s? This can't be undone.", "%s을(를) 삭제할까요? 되돌릴 수 없습니다."},
    {"Deleted %s", "%s을(를) 삭제했다"},
    {"Cannot delete %s: %s", "%s을(를) 삭제할 수 없다: %s"},
    {"Cancel", "취소"},
    {"The editor isn't built in %s (build\\Editor\\bin)", "%s에 빌드된 에디터가 없다 (build\\Editor\\bin)"},
    {"Cannot start the editor: %s", "에디터를 실행할 수 없다: %s"},
    {"Opened %s in the editor", "%s을(를) 에디터에서 열었다"},
};

bool g_korean = true;

} // namespace

void setLanguage(const std::string& language) { g_korean = language != "en"; }

const char* tr(const char* english) {
    if (!g_korean) return english;
    for (const Pair& p : kKorean)
        if (std::strcmp(p.en, english) == 0) return p.ko;
    return english;
}

std::string trf(const char* english, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, english);
    std::vsnprintf(buffer, sizeof(buffer), tr(english), args);
    va_end(args);
    return buffer;
}

} // namespace mkqa
