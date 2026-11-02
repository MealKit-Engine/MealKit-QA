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
    {"Reports", "리포트"},
    {"Tests (%u)", "테스트 (%u)"},
    {"Save as Scenario", "시나리오로 저장"},
    {"Turns the recording into a scenario test in the project's tests/ folder, with every state value at the report's tick as checks. Keep the checks that show the bug.", "녹화를 프로젝트 tests/ 폴더의 시나리오 테스트로 만듭니다. 리포트 시점의 모든 상태 값이 검사로 들어갑니다. 버그를 보여 주는 검사만 남기세요."},
    {"not run", "실행 안 함"},
    {"running", "실행 중"},
    {"passed", "통과"},
    {"failed", "실패"},
    {"Build the game first (build.bat in %s): no executables in build\\Release\\bin", "먼저 게임을 빌드하세요(%s의 build.bat): build\\Release\\bin에 실행 파일이 없습니다"},
    {"running...", "실행 중..."},
    {"Cannot save the scenario: %s", "시나리오를 저장할 수 없습니다: %s"},
    {"Saved tests/%s.scenario. Keep the checks that show the bug.", "tests/%s.scenario를 저장했습니다. 버그를 보여 주는 검사만 남기세요."},
    {"Run All", "모두 실행"},
    {"Stop", "중지"},
    {"Cancels the waiting runs (the running one finishes)", "대기 중인 실행을 취소합니다(실행 중인 것은 끝까지 갑니다)"},
    {"%u passed, %u failed", "%u개 통과, %u개 실패"},
    {"No scenario tests in tests/. Save one from a report ([Save as Scenario]) or from editor play.", "tests/에 시나리오 테스트가 없습니다. 리포트([시나리오로 저장])나 에디터 플레이에서 저장하세요."},
    {"Scenario", "시나리오"},
    {"Result", "결과"},
    {"Multiplayer test (mp_test)", "멀티플레이 테스트 (mp_test)"},
    {"clients", "클라이언트"},
    {"ticks", "틱"},
    {"Run mp_test", "mp_test 실행"},
    {"Host + clients with bot input on this PC, headless: passes when every peer has the same state hash", "이 PC에서 호스트와 클라이언트를 봇 입력으로 창 없이 실행합니다. 모든 피어의 상태 해시가 같으면 통과입니다"},
    {"Pick a scenario. Double-click runs it.", "시나리오를 고르세요. 더블클릭하면 실행합니다."},
    {"%u check(s), %u failed", "검사 %u개, 실패 %u개"},
    {"Run", "실행"},
    {"Update Expected", "기대값 갱신"},
    {"Rewrites the == values in the file with the current results (when the rules changed on purpose)", "파일의 == 값을 지금 결과로 다시 씁니다(규칙을 일부러 바꿨을 때)"},
    {"File", "파일"},
    {"Output", "출력"},
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
