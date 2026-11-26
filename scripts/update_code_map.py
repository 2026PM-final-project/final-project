"""Regenerate README regions and symbol line references using only Python stdlib."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE_FILES = [
    "src/game.hpp", "src/game.cpp", "src/main.cpp", "src/movement.hpp",
    "src/renderer.hpp", "src/viewport.hpp", "src/speed_controls.hpp", "src/renderer.cpp", "src/retro.cpp",
    "src/audio.hpp", "src/audio.cpp", "src/scores.hpp", "src/scores.cpp",
    "tests/test_game.cpp", "tests/test_controls.cpp",
]
DESCRIPTIONS = {
    "src/retro.cpp": ["1122(4) 색상·입체 면·방향광·반사광을 64x64 RGBA 텍스처로 캐시, 글자 중앙 정렬", "연속 실수 좌표를 따르는 구형 캐릭터, 아이템 팝업, 화염, 출구, F2 8칸 탐색 영역·최단 경로", "1UP·HIGH SCORE·STAGE 헤더, 목숨·F/B/S 배지, 플레이어별 강화·적 구분·실시간 속도 슬라이더 HUD", "타이틀 로고·미리보기 맵·시작 안내의 홈 화면", "STAGE # 준비 화면, 도움말·랭킹·클리어·재시작 오버레이"],
    "tests/test_controls.cpp": ["실제 main.cpp 키 메시지 핸들러를 데스크톱 입력 주입 없이 호출하는 테스트 하네스", "C01~C21: 홈·STAGE 시작, 방향키 4개·연속 이동·WASD/2인 입력 제거, H/R/N·별칭·다음 단계·재시작·원격 폭탄·폭탄 칸 재진입·V 키 무동작·창 비율 유지·속도 슬라이더 입력·테스트 판 랭킹 제외·최종 클리어 timestamp 저장·공통 경로의 복원 기록 유지"],
    "src/speed_controls.hpp": ["4종 적 속도 슬라이더·기본값 복원 버튼의 공통 좌표와 마우스 히트 테스트"],
    "src/movement.hpp": ["렌더 프레임의 새 키 입력을 첫 물리 틱에서만 소비, 유지 입력·칸 중심 방향 버퍼 처리"],
    "src/game.hpp": ["상수·열거형·좌표·플레이어·적·폭탄·화염·입력 데이터 선언", "게임 상태와 공개 API, 내부 시드·이동·파괴 함수 선언"],
    "src/game.cpp": ["격자 방향 연산, 칸 중심·기본 속도·실시간 속도 프로필, 문자열·좌표 접근", "고정 시드 맵 생성, 시작 안전구역, 숨은 아이템·출구, 단계 전환", "6개 연습 장면의 명시적인 초기 배치와 능력 설정", "플레이어 폭탄 통과·적 장애물 판정, BFS 거리·선행 노드·경로 복원, 적의 방향 선택", "목표 중심까지 등속 이동, 벽 충돌과 적의 새 폭탄 회피", "소유자별 폭탄 용량·중복 설치 제한, 원격 기폭·종류 순환, 블록 파괴·아이템 노출", "폭발 큐·중복 제거·벽 스냅샷·블록 칸 화염·폭발 waveId·즉시 연쇄반응", "접촉 기하, 적 피해, 목숨·리스폰, 아이템 효과, 열린 출구 판정", "고정 틱 갱신 순서, 아이템 등장 애니메이션·타이머·도화선·정지, 이벤트 수거", "F2 표시용 전체 8칸 BFS 거리 지도"],
    "src/main.cpp": ["앱 소유권, 마우스 속도 슬라이더·F6 패널 전환, 홈·STAGE 준비·기존 명령", "방향키 전용 1인 입력, 수정된 방향 버퍼 재사용, 기록 저장 조건", "Win32 창 메시지, 키 반복 방지, 비율 유지 마우스 좌표·드래그 캡처·포커스 상실 처리", "창 생성·OpenGL/오디오 초기화, 120Hz accumulator, 60FPS 제한, 자원 해제"],
    "src/renderer.hpp": [""],
    "src/viewport.hpp": ["1280×900 가상 캔버스 비율을 유지하는 중앙 뷰포트와 여백 계산"],
    "src/renderer.cpp": ["2D WGL 자원·폰트·텍스처 생성, 비율을 유지하는 리사이즈·자원 해제", "2D 직교 좌표와 글꼴 출력", "2D 홈·보드·HUD·오버레이 합성", "OpenGL front buffer 읽기와 BMP 파일 저장"],
    "src/audio.hpp": ["별도 스레드·원자 플래그·mutex 이벤트 큐를 가진 오디오 인터페이스"],
    "src/audio.cpp": ["스레드 시작·종료, 음소거, 게임 이벤트를 사운드 큐로 전달", "waveOut 3중 버퍼, 배경 선율, 이벤트별 주파수·노이즈·감쇠 포락선 믹싱"],
    "src/scores.hpp": ["최종 클리어·확인된 복원 기록, 읽기·저장·완료 시각·공통 저장 경로 선언"],
    "src/scores.cpp": ["최종 클리어·달력·24시간제 시각 검증, 구형 기록 제외·명시적으로 확인된 날짜 전용 기록 보존, 상위 10개 읽기", "임시 파일 완성 후 교체, 기존 파일 보존, 로컬 완료 시각 생성(초 단위)·빌드 간 공통 저장 경로 선택"],
    "tests/test_game.cpp": ["테스트 하네스·실패 집계·독립 빈 맵·시간 진행 도우미", "T01~17: 시작·이동·설치·폭발·연쇄반응", "T18~36: 적 이동·8칸 BFS 경계·피격·목숨·숨은 출구·승리", "T37~58: 아이템·단계·특수 폭탄·1인 고정·장갑/고속 적·정지·랭킹·회귀·장시간 검증", "T59~62: 여러 물리 틱의 짧은 입력 중복 방지, 연속 이동·방향 전환·연속 탭 입력 회귀", "T63~73: 직진 적·BFS, Fire 획득별 범위 증가, 탐색 영역·팝업 시간, 부서진 칸 화염·연쇄폭발 아이템 보호·화염 접촉", "T74~76: 여러 폭탄 연속 통과, 이동 중 나타난 폭탄 통과, 재진입 후 폭발 피해", "T77~82: 적 종류별 속도·실제 이동·정지/재개·범위/간격·설정 유지·기본값 복원", "T83~87: 구형 기록 제외, 날짜·시각 검증, 확인된 날짜 전용 기록의 재시작 보존·공통 저장 경로"],
}

def generate():
    result = ["| 파일 / 주석 | 현재 줄 범위 | 해당 코드의 역할 |", "|---|---|---|"]
    symbols = []
    for file in SOURCE_FILES:
        lines = (ROOT / file).read_text(encoding="utf-8").splitlines()
        markers = [(i + 1, m.group(1)) for i, s in enumerate(lines)
                   if (m := re.search(r"// \[(\d+)\]", s))]
        if not markers:
            result.append(f"| [{file}]({file}) | L1~{len(lines)} | 렌더러 API, 화면 상태·핸들·텍스처·폰트 선언 |")
        else:
            if markers[0][0] > 1:
                result.append(f"| [{file}]({file}) / 공통 선언 | L1~{markers[0][0]-1} | 헤더 포함, 컴파일 옵션 매크로, namespace 및 공통 선언 |")
            for k, (start, number) in enumerate(markers):
                end = markers[k+1][0]-1 if k+1 < len(markers) else len(lines)
                description = DESCRIPTIONS[file][k]
                result.append(f"| [{file}]({file}#L{start}) / [{number}] | L{start}~{end} | {description} |")
        if file.endswith(".cpp") and not file.startswith("tests/"):
            for i, line in enumerate(lines):
                match = re.search(r"\b((?:Game|Actor|Enemy|Renderer|Audio)::\w+)\s*\(", line)
                if match and not line.startswith(" "):
                    symbols.append((match.group(1), file, i+1))
    for file, desc in [("CMakeLists.txt", "코어·게임·테스트 빌드 대상, 시스템 라이브러리, CTest 등록"),
                       ("build.ps1", "GCC 탐색 → 규칙 테스트 빌드·실행 → GUI 정적 링크"),
                       ("Run Demo.cmd", "소스에 맞는 실행 파일을 찾는 run.ps1 호출"),
                       ("run.ps1", "소스보다 새 실행 파일 선택, 필요 시 GCC/MSBuild 빌드 후 실행"),
                       ("scripts/package.py", "검증된 소스·문서·실행 파일 묶기, ZIP 무결성 검사·SHA256"),
                       ("scripts/update_code_map.py", "README 영역·함수 줄 번호 자동 갱신")]:
        count = len((ROOT / file).read_text(encoding="utf-8").splitlines())
        result.append(f"| [{file}]({file}) | L1~{count} | {desc} |")
    result += ["", "### 함수 찾기", "", "알고리즘·요구사항 표에서 사용한 주요 함수의 실제 시작 행입니다.", "",
               "| 함수 | 위치 |", "|---|---|"]
    for symbol, file, line in symbols:
        result.append(f"| `{symbol}` | [{file}:L{line}]({file}#L{line}) |")
    result += ["", "`Application::start/commands`와 `Application::input/frozen/record`는 `main.cpp`의 [01]/[02]에 인라인 정의돼 있습니다. `localTimestamp/readScores/saveScore/scoreFileForExecutable`는 `scores.cpp`의 [01]/[02]에 있습니다.", ""]
    return "\n".join(result)

if __name__ == "__main__":
    path = ROOT / "README.md"
    text = path.read_text(encoding="utf-8")
    start, end = "<!-- CODE_MAP_START -->", "<!-- CODE_MAP_END -->"
    content = generate()
    text = text[:text.index(start)+len(start)] + "\n\n" + content + text[text.index(end):]
    path.write_text(text, encoding="utf-8")
    print("README code regions and symbol lines updated.")
