# 터미널 입출력 통일안 (dasm / dlnk / hexdump)

2026-10-11 작성, 같은 날 구현 완료 (아래 "구현 결과" 참고). 관련 todo: `todo.md`의 `-align` 항목.

## 구현 결과 (2026-10-11)
- `common/iotool.hpp`
  - `errPosGenerator(file, line, row)` / `(file, line)` / `(line)` → `file:line:row` / `file:line` / `line N`.
  - `Terminal::TerminalIO<FlagType>`: 2절 인자 규칙의 파서. `FlagData{text, type, hasvalue, follow, description}`, `setup(flags, count, seperator, synopsis)`, `parse(...)`(argv / 인자 목록 / 문자열 하나), `has`, `value`, `usage`, `printHelp`, `printParseError`.
    - `follow`는 "같이 켜지는 플래그", `seperator`는 문자열 하나로 받을 때의 인자 구분 문자(큰따옴표로 묶기 가능)로 정함.
  - 공유 오류: `codeError`, `toolError`, `readFailed`, `writeFailed`, `noInput`, `invalidValue`, `printError`, `printDiagnostic`, `usageError`, `parseNumber`, `EXIT_OK/EXIT_FAIL/EXIT_USAGE`. 프로그램 고유 오류는 각 프로그램에서 만든다.
- dasm / dlnk / hexdump의 main이 모두 이 파서를 쓴다. 위치 오류는 3번(GCC 형식)까지 반영됨.
- hexdump: `-o` 추가, 파일 읽기는 `iotools`. `-width`만 주면 묶음 크기도 width를 따른다(예전 빌드는 항상 4로 묶였음).
- 이후 추가된 옵션 (2026-10-12 ~ 10-17): 세 도구 공통 `-big`/`-little`, dlnk `-org N`, `-raw`, `-start <label>`. 현재 dlnk 형식: `<in...> [-o out] [-big|-little] [-org N] [-padbss] [-raw | -start <label>] [-v]`. 설명은 `SYNTAX.md` 1·9절.
- `-v`는 dlnk에만 있다. 값 생략 옵션(`-align=8`)은 아직 파서에 없다 (`-align` 구현 때 추가).

## 1. 구현 전 상태 (기록용)

| | dasm (`assembler/main.cpp`) | dlnk (`linker/main.cpp`) | hexdump (`hexdump/hexdump.cpp`) |
|---|---|---|---|
| 형식 | `<in> [-o out] [-file -token -preproc -eval -bin]` | `<in...> [-o out] [-padbss]` | `<file> [-nodec] [-width N] [-bundle N]` |
| 인자 순서 | 자유 | 자유 | 파일이 반드시 첫 번째 |
| 기본 출력 | `<입력>.o` | `a.out` | stdout |
| 오류 출력 | stdout | stdout | stdout |
| 오류 문구 | `unknown flag 'x'` | `unknown flag 'x'`, `link failed` | `wrong flag`, `wrong input`, `failed to open file` |
| 성공 시 출력 | 없음 | `linked N file(s) -> ...` | 덤프 |
| 종료 코드 | -1 (셸에서 255) | -1 | -1 |
| 도움말 | 인자가 없을 때만 | 인자가 없을 때만 | 인자가 없을 때만 |

### hexdump의 기존 버그
- `-width` / `-bundle` 뒤에 값이 없으면 오류만 출력하고 계속 진행해 `argv[argc]`(NULL)를 읽는다 → 크래시.
- `-width ""`(`convStr`이 0 반환)나 `-bundle 0`이면 `count % bundle`에서 0으로 나눈다.
- 오류가 stdout으로 나가서 `hexdump x > out.txt`의 결과 파일에 섞인다.

## 2. 통일안

### 인자 규칙
- 형식: `tool [옵션] <입력...>`. 옵션과 입력의 순서는 자유.
- 옵션 이름은 지금처럼 대시 하나 + 긴 이름 (`-o`, `-padbss`, `-width`). 기존 이름은 바꾸지 않는다.
- 값을 받는 옵션은 다음 인자를 값으로 쓴다: `-o a.o`, `-width 16`.
- 값을 생략할 수 있는 옵션(`-align`)은 `=` 형식으로만 값을 받는다.
  - `-align` → 4, `-align=8` → 8.
  - `-align 8`을 허용하면 `8`이 값인지 입력 파일인지 애매해지므로 허용하지 않는다.
- 숫자 값은 공용 함수 하나로 해석한다.
  - 10진수와 `0x` 16진수, 범위 검사, 실패 시 사용법 오류.
  - hexdump의 크래시와 0 나누기가 이걸로 해결된다.
- `-h` / `-help`: 사용법을 stdout에 출력하고 종료 코드 0.
- `-little` / `-big`: 세 도구 공통 바이트 순서 옵션, 기본 `-little`. 둘을 같이 주면 사용법 오류(종료 코드 2). 의미는 `SYNTAX.md` 1절.
- 옵션 집합은 `u64` 비트 집합이라 도구마다 옵션을 64개까지 둘 수 있다.

### 출력 스트림
- stdout: 사용자가 요청한 결과만 (dasm의 `-token`/`-bin` 등 덤프, hexdump 결과, 도움말).
- stderr: 진단만 (오류, 경고, `-v`일 때의 진행 정보).
- 성공 시 기본은 아무것도 출력하지 않는다. dlnk의 `linked N file(s)...`는 `-v`로 옮긴다.

### 메시지 형식
- 도구 자체 오류: `<도구>: error: <내용>`
  - 예: `dlnk: error: undefined symbol 'foo' (used in 'b.o')`
  - 예: `hexdump: error: invalid value for -width: 'abc'`
- 소스 위치가 있는 오류는 GCC 형식 (VS Code 등이 링크로 인식):
  ```
  path/a.txt:12: error: MISSING_EXPRESSION: bcdTest
      st.zero gen[7].32B, bcdTest
  ```
- 현재 위치 오류 문구(`error at <파일> line N: ...`)는 `assemble()` 안에서 만들어지므로, 이 형식으로 바꾸려면 `assembler.hpp`도 수정해야 한다.

### 종료 코드
| 코드 | 의미 |
|---|---|
| 0 | 성공 (`-h` 포함) |
| 1 | 처리 실패: 입력을 못 읽음, 어셈블/링크 오류 |
| 2 | 사용법 오류: 모르는 플래그, 값 누락, 입력 없음 |

### 출력 파일
- 세 도구 모두 `-o`로 지정한다.
- 기본값: dasm은 `<입력>.o`, dlnk는 `a.out`, hexdump는 stdout.

## 3. 구현 위치
- `common/cli.hpp` 신설: 옵션 표(이름, 값 필요 여부, 설명)를 받아 파싱, 사용법 출력, 오류 메시지, 종료 코드를 한 곳에서 처리하는 작은 파서.
- dasm, dlnk, hexdump 모두 `../common/cli.hpp`로 include한다.
  - 2026-10-11 저장소 루트를 `src/`로 옮기면서 `hexdump/`와 `common/`이 나란히 있게 되어, `digital computer 32/tools`의 미러 구조와 같은 상대경로가 된다.
  - hexdump용 빌드 스크립트(`build-hex.cmd`)를 `build-asm.cmd`와 같은 패턴으로 추가한다.

## 4. 진행 순서
1. ~~공용 파서를 만들고 세 main에 적용~~ (`common/cli.hpp` 대신 `common/iotool.hpp`의 `Terminal`에 넣음)
2. 이 파서 위에서 `-align`을 구현한다. (남음)
3. ~~위치 오류를 GCC 형식으로~~
4. ~~`SYNTAX.md` 갱신, 배포~~
