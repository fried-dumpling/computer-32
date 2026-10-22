# 툴체인 내부 구조 (dasm / dlnk / hexdump)

`tools/`의 어셈블러·링커·hexdump 소스를 고칠 때 보는 문서입니다. 사용법과 문법은 `document/SYNTAX.md`, 명령어 의미는 `document/ISA.md`에 있습니다.

> 2026-10-23 정리: 예전 `assembler-linker` 저장소(`computer 32 (new)/src`)의 `CLAUDE.md`(작업 기록 1~70번), `todo.md`, `BUGS.md`, `CLI.md`, `README.md`를 합쳐 지금도 유효한 내용만 남겼습니다. 남은 할 일은 `document/todo.md` "1. 어셈블러 / 링커"로 옮겼습니다. 원문은 git 히스토리에 있습니다(`git show 508f47c:tools/CLAUDE.md`).

## 1. 파일 구성

```
tools/
  assembler/   assembler.hpp   ISA-32 어셈블러 본체 (lexer/preprocesser/parser/evaluator/format 네임스페이스)
               lexer.hpp       정규식 → DFA 렉서 생성기 (제네릭, maximal munch, 같은 길이면 앞 규칙 우선)
               parser.hpp      LALR 파서 생성기 (제네릭, CreateData 문법 표)
               evaluator.hpp   AST 순회 평가기 생성기 (제네릭)
               preprocesser.hpp  토큰 단위 전처리기 생성기 (제네릭)
               main.cpp        dasm 진입점
  linker/      linker.hpp      dlnk 본체 (evaluate / stackMachine 네임스페이스)
               stackmachine.hpp  제네릭 RPN 실행기 StackMachine<Operation>
               main.cpp        dlnk 진입점
  common/      format.hpp      오브젝트 파일 형식 (namespace format_data) — dasm/dlnk 공유
               iotool.hpp      파일 IO, 경로, 터미널 인자 파서·오류 출력 (namespace iotools)
  hexdump/     hexdump.cpp
  emu/         isa32emu.py     명령어 수준 에뮬레이터 (CLAUDE.md "Emulator")
  dsim/        헤드리스 회로 시뮬레이터 (tools/dsim/README.md)
  build-asm.cmd / build-lnk.cmd / build-hex.cmd   → bin/dasm.exe, bin/dlnk.exe, bin/hexdump.exe
```

- 모든 소스는 헤더 온리이고 `main.cpp` 하나를 `g++ -std=c++17 -O2`로 컴파일합니다. 빌드 스크립트는 자기 위치 기준 상대경로를 쓰고, `g++`가 PATH에 없거나 실패하면 exit 1.
- 빌드 스크립트는 **PowerShell에서 호출**합니다. git-bash에서 `cmd //c`로 부르면 인자 따옴표가 꼬여 실패합니다.
- 회귀용 옛 소스는 `program/archive/*.txt`입니다(예전 `tools/test/program`). 그중 parse 실패(branch, factorial_norec, prime_old — LALR 문제, 보류)와 소스 오류(bcdtodec: 없는 라벨, texteditor: `#define`을 `#macro`로 써야 함)는 실패하는 게 정상입니다. 문법/인코딩을 바꾸면 이 소스들과 `program/`, `boot/`의 `.asm`이 이전 dasm과 바이트 단위로 같은지 확인합니다.

## 2. 파서 테이블 캐시

- dasm은 처음 실행할 때 렉서/파서 테이블을 만들어 실행 파일 옆 `<exe 이름>_table.bin`(`bin/dasm_table.bin`)에 저장하고 이후엔 그걸 읽습니다.
- 캐시는 `assembler.hpp`의 `cache` 네임스페이스 `const unsigned long long version`(현재 **415**)으로만 무효화됩니다. **렉서 규칙, 문법, `TokenType`/`NonterminalType`/`ASTNodeType` enum을 바꾸면 반드시 version을 올립니다.** 안 올리면 이전 문법의 stale 테이블이 그대로 쓰여 소스와 실행 결과가 어긋납니다.
- 실행 파일을 교체할 때도 `*_table.bin`을 지우고, 실제 파일을 한 번 어셈블해서 다시 만들게 합니다(인자 없이 실행하면 테이블을 안 만듭니다). dlnk는 테이블이 없습니다.
- 파서 생성기는 충돌(shift/reduce, reduce/reduce)이 나도 **경고 없이 먼저 채워진 액션을 유지**합니다(`if (slot.type == ActionType::Error) slot = ...`). 애매한 문법을 넣으면 조용히 틀린 트리가 나올 수 있으니, 규칙을 추가할 때 lookahead가 겹치지 않는지 직접 따져 봅니다.

## 3. 어셈블러 (dasm)

### 3.1 파이프라인 (`assemble()`)

`lex → preprocess(#include 포함) → parse → evaluate → formatData → Format::getRaw`

- 실패 여부는 `AssemblerDump::errorCode`의 비트(`FAIL_LEX`, `FAIL_PREPROCESS`, `FAIL_PARSE`, `FAIL_EVALUATE`, `FAIL_FORMAT`)로 기록되고, 덤프 플래그는 `AssemblerDump::flags`(`getToken`, `getPreproc`, ...)입니다. 두 enum 모두 1부터 시작하는 비트 체인입니다(예전에 0에서 시프트해 전부 0이던 버그가 있었음).
- 콜백·드라이버는 `int`(0 = 성공, 그 외 = 단계별 오류 enum)를 반환하고 그대로 위로 전파합니다: `PreprocessError`, `EvaluateError`(+ `preprocessErrorName()`/`evaluateErrorName()`).
- 위치가 있는 오류는 `describeError`가 `<file>:<line>: error: <detail>\n    <소스 줄>`로 만듭니다. 위치는 토큰의 `fileId`/`fileLine`, 소스 줄은 `tokenBackup`(전처리 전 토큰, `std::multimap`이라 줄 안 순서 유지)에서 재구성합니다.

### 3.2 렉서·전처리기

- 토큰 3개 필드: `line`(전 파일 통틀은 절대 줄), `fileLine`(파일 안 줄), `fileId`(`includedFiles` 인덱스). `tagLine`이 태깅하고, `tokenBackup` 복사는 반드시 태깅 **후**에 합니다.
- 0x80 이상 바이트(UTF-8)는 DFA에서 127로 조회합니다 → 주석 안에서는 허용, 코드에선 unknown 토큰.
- `#include<path>`: `handleReplace`가 `includeCallback`(소유형 `FunctionRef`, 람다를 힙에 복사)을 그 자리에서 호출해 파일을 읽고 lex + 공백 제거까지 끝난 토큰을 받아 바로 끼워 넣습니다. 지우기와 끼워 넣기는 한 호출 안에서 합니다(나눠 두면 `#include`가 마지막 토큰일 때 `erase()`가 `end()`를 돌려줘 끼워 넣기가 사라짐).
  - 경로는 `normalizePath`(`.`/`..` 접기, 출력 구분자 `\`)로 정규화해 `includedFiles`(set)에 넣고, 이미 있으면 `DUPLICATE_INCLUDE`(중복·순환·자기 자신 모두). 입력 파일은 `main`에서 `iotools::absolutePath`(cwd 기준)로 절대경로화.
  - 읽은 텍스트가 `\n`으로 안 끝나면 하나 붙여 줄 계산이 어긋나지 않게 합니다.
- `#ifdef`/`#ifndef`/`#end`: `PreprocData::ifOpen`(열린 블록의 여는 토큰 스택)과 `removeDepth`(제거 중인 블록 안 중첩 지시어 깊이). 매크로 본문 안의 조건부는 확장 시점에 평가됩니다.
- 매크로: 닫는 `)`에서 "쉼표가 있었거나 인자 토큰이 있으면" 마지막 인자를 넣고, 개수가 다르면 `MACRO_ARG_COUNT_MISMATCH`. 인자 0개 매크로는 `(` 없이도 즉시 확장, 인자 있는 매크로가 `(` 없이 오면 `EXPECTED_MACRO_CALL`. 확장은 `expandMacro()`.

### 3.3 문법

- `CreateData` 항목: `{ {Nonterminal, ASTNodeType, nodeIndex}, { {symbol, keep, splice}, ... } }`. `nodeIndex`는 노드 `.type`/`.text`를 가져올 RHS 위치(-1이면 ASTNodeType 그대로), `keep`은 자식으로 남김, `splice`는 그 자식의 하위 노드를 부모에 병합(리스트 누적).
- AST 노드의 `srcToken`은 리듀스 시 가장 왼쪽 심볼의 토큰(값 복사)입니다.
- 평가기 `switch(cur->type)`에서 AST 노드 종류는 `(u64)ASTNodeType::X + (u64)TokenType::__end`로 비교하고, 토큰은 `(u64)TokenType::X` 그대로 비교합니다. 오프셋을 빠뜨리면 다른 노드와 우연히 겹쳐서 일부만 잡힙니다(예전 `.data`+`.text` 섹션 1개 버그의 원인).
- 식: `expressionL0`(괄호, 리터럴, 식별자, `$`) ~ `expressionL13`(`?:`)의 14단계 체인으로 우선순위 선언 없이 충돌을 피합니다. 순서는 `SYNTAX.md` 5절. `+`/`-`는 단항(L1)과 이항(L4)이 같은 토큰이라 평가기에서 `child.size() == 1`로 구분합니다.
- 섹션: `section -> section_unit section(splice) | section_unit`, `section_unit(AT::*_section) -> .text [identifier] [본문(splice)]` — 섹션당 노드 하나, 이름은 child[0]가 `TokenType::identifier`일 때만 이름입니다. `global`/`extern`/`global section`은 `outer_section`(OUTER 섹션, 링커가 건너뜀).

### 3.4 평가기 (`evaluator::functions`)

순서: `calculateSection → calculateAddr → evaluateCode → collapseExpr`

- `ExprData{type, value, coeff, operations}`
  - `CONSTANT`: 값 확정. `LINEAR`: `value + Σ coeff[section_id] · base(section)`(섹션 베이스에 대해 아핀). `COMPLEX`: 그 외 — 값 없이 `operations`(RPN)만 링커로 넘김.
  - `LinearKind`: `ADD_LIKE`(LINEAR±CONST, LINEAR+LINEAR), `SUB_LIKE`(LINEAR-LINEAR는 계수 차감, 0이면 CONST), `SCALE_LIKE`(×CONST), `NEG_LIKE`(단항 -). 나머지 조합은 COMPLEX.
  - 단항 `+`는 passthrough(연산을 남기지 않음). `?:`는 조건이 CONST면 폴딩, 아니면 COMPLEX + `SELECT`.
- `calculateSection`: 노드를 섹션에 매핑(`sectionIdMap`). 라벨·명령어뿐 아니라 `allocate`류·`curline`도 `sectioningArr`에 넣어야 올바른 섹션에 들어갑니다. 새 섹션을 만들 때 `sections`와 `sectionByte`를 같이 push.
- 식 처리(연산자/리터럴/identifier/index)는 공용 `calculateExpr`이고 두 곳에서 부릅니다.
  - `calculateAddr`: 주소 결정(명령어 4바이트, 라벨 정의, allocate 크기 — 크기는 반드시 CONSTANT). 전방 참조는 조용히 건너뜀. 라벨 값은 섹션 안 오프셋, `coeff[section]=1`.
  - `evaluateCode`: 값이 없는 노드를 다시 계산(이 시점엔 모든 라벨이 있음) 후 인코딩. 끝내 못 푼 값은 소비자(`immidate16`/`immidate8`/`reg_index`/`allocate`)가 `MISSING_EXPRESSION`.
  - 라벨 주소는 고정 크기 명령어와 상수 allocate로만 정해지므로 주소 패스는 1회로 충분합니다.
- `equ`: 값이 있으면 `idenifierValue[name]`에 ExprData 복사(사용처는 식을 복사하므로 `K equ later+4`도 재배치로 처리). 한계: equ보다 앞에서 쓰이고 equ 식이 전방 라벨에 의존하면 실패.
- `$`: 현재 섹션 위치로 LINEAR를 만들고 숨은 라벨 `#curline#N`으로 등록해 일반 라벨처럼 내보냅니다.
- `extern` 이름은 `{COMPLEX, 0, {}, [IDENTIFIER name]}` → 항상 재배치. 검사: `GLOBAL_UNDEFINED`, `EXTERN_DEFINED`, `GLOBAL_SECTION_UNDEFINED`.
- 재배치 기록: 각 값은 `.data` + `std::vector<ReplaceData{index, offset, size}>`(비트 단위). 명령어는 `appendReplace(dest, src, shift)`로 필드 위치만큼 offset을 보정해 합치고, push 직전 `registerReplace`가 섹션 바이트 위치를 키로 `Section::operationMap`에 등록합니다. 키: big = `start + byte - 1 - offset/8`, little = `start + offset/8`.
- `collapseExpr`: 같은 식을 하나로 합칩니다(`EqualOperation`/`HashOperation`은 `type`/`value`/`text` 모두 비교). `formatData`는 `compressionIndex`로 압축 후 인덱스를 씁니다.
- 데이터: `pushByteN`은 빅엔디안 바이트열을 만들고 little이면 뒤집습니다. 5바이트 이상은 끝 기준 반복. 비상수 값의 패치 크기는 `min(32, 바이트×8)`, 위치는 첫 워드. 목록(`total(element), {...}`)은 원소마다 같은 방식, 문자열은 `string2bytes`(이스케이프 `\n \t \r`, 10진 최대 3자리) 후 0 패딩.

### 3.5 오브젝트 형식 (`common/format.hpp`)

- 헤더 14필드(56바이트, 첫 필드는 헤더 크기): 6개 구역 `sections`/`binary`/`mapper`/`expressions`/`definedIdentifier`/`usedIdentifier`의 start/end + `endian`(LITTLE=0, BIG=1). 메타데이터 자체(`push32`/`read32`)는 항상 빅엔디안.
- `Section{name, type, start, end, size}` — `size`는 주소 공간(bss 포함). `Mapper{sectionName, byteIndex, offset, size, exprIndex}`. `Identifier{name, section, sectionIndex, value}`(상수면 `sectionIndex = (u32)-1`). `expressions`는 `Operation` RPN 목록이 `EOX`(type 0)로 끝남. 문자열은 NUL 종단.
- `Format::loadRaw`는 `bool`: 헤더 크기, 구역 연속성·범위, 레코드 경계(NUL, 고정 필드, EOX), 섹션 범위를 검사합니다. 실패하면 dlnk가 `invalid object file`.
- **형식을 바꾸면 예전 `.o`는 못 읽습니다** — 다시 어셈블해야 합니다.
- 이름 mangling(`formatData`, `mangleLabel`): 라벨·섹션 이름은 `<출력 오브젝트 base name>#<이름>`(빈 섹션 이름도 `file#`). `global` 라벨과 `global section`은 원래 이름, 재배치 식의 이름은 global 또는 extern이면 원래 이름. 그래서 링크할 오브젝트들은 파일 이름이 달라야 합니다.

## 4. 링커 (dlnk)

`link()` → 각 오브젝트 `loadRaw` → `evaluate::functions::evaluate()`:

1. `convertData`: 오브젝트 → `EvalData::files`. 파일 안에서 같은 (type, name) 섹션을 합치고(`FileData::sections` + `nameMap`, 키 `"<type>#<name>"` — 이름만 키로 쓰면 이름 없는 `.data`와 `.text`가 섞임), 바이트는 공용 `initialBin`에 한 번만 붙이고 섹션은 `Indexer`(구간 목록)로 참조. 식별자는 `pendingLabels`(섹션 + 로컬 오프셋)로 보류.
2. `mergeByName`: 전 파일에 걸쳐 같은 (type, name)을 `EvalData::sections`로 병합(mangling 때문에 실제로는 `global section`만 파일 간 병합).
3. `mergeByType`: TEXT → DATA → BSS 순 배치, `-org N`이면 첫 그룹이 N에서 시작. 크기는 바이트 수가 아니라 `SectionData::size`(bss는 주소만 차지, `-padbss`면 0으로 채움). 보류된 식별자를 절대 주소로 풀어 `definedLabel`.
4. `checkDuplicateSymbols`: 같은 이름이 두 파일에 있으면 `duplicate symbol`.
5. `mapExpr`: 파일별 `usedIdentifier`를 전 파일 `definedLabel`에서 해석(없으면 `undefined symbol`), 각 재배치 식을 `castOperation`(포맷 Operation → `Element`, IDENTIFIER는 주소로, 연산자는 arity) → `stackMachine::runOperation`으로 계산해 `totalBin`에 비트 단위로 패치(big은 앞 바이트로, little은 뒤 바이트로 확장). 바이너리 위치는 `sectionOffset - origin + 오프셋`.
6. `addHeader`(기본): `{시작 라벨 주소, 프로그램 길이}`를 앞에 붙임. `findStartLabel`은 global 이름을 먼저, 없으면 한 파일에만 있는 지역 라벨. `-raw`면 생략.

- `stackMachine::function()`은 산술을 `s32`로 계산해 `u32`로 되돌립니다(비트 연산·동등 비교는 `u32`). 오류: `DIV_BY_ZERO`, `MOD_BY_ZERO`, `UNKNOWN_OPERATOR`, `STACK_UNDERFLOW`, `INVALID_RESULT`.
- 반복자 이름 규칙(`linker.hpp`): for문 반복자는 중첩 깊이로 `it`, `si`, `ti`, `qi`, `pi`, `hi`. 루프가 아닌 `find` 결과는 용도 이름(`slotIt`, `mergedIt`).

## 5. 터미널 입출력 (`common/iotool.hpp`)

세 도구가 같은 규칙을 씁니다(사용자 쪽 설명은 `SYNTAX.md` 1·10절).

- `Terminal::TerminalIO<FlagType>`: `FlagData{text, type, hasvalue, follow, description}` 표로 `setup` → `parse`(argv / 인자 목록 / 문자열 하나) → `has`/`value`/`usage`/`printHelp`/`printParseError`. 플래그 집합은 `u64` 비트(도구당 최대 64개).
- 값은 `-o out` / `-o=out`, 숫자는 `parseNumber`(10진, `0x`, 범위 검사). 값을 생략할 수 있는 옵션(`-align=8` 형식)은 아직 파서에 없습니다(`-align` 구현 때 추가 — `-align 8`은 입력 파일과 헷갈리므로 `=`만 허용하기로 함).
- 공유 오류 함수: `codeError`, `toolError`, `readFailed`, `writeFailed`, `noInput`, `invalidValue`, `usageError`, `printError`, `printDiagnostic`. `errPosGenerator`가 GCC 형식 위치(`file:line[:row]`)를 만듭니다.
- stdout = 요청한 출력만(덤프, 도움말), stderr = 진단. 성공하면 기본 무출력(dlnk 요약은 `-v`).
- 종료 코드: `EXIT_OK` 0, `EXIT_FAIL` 1(입력·어셈블·링크 실패), `EXIT_USAGE` 2(명령줄 오류).
- 출력 파일: `-o`, 기본은 dasm `<입력>.o`, dlnk `a.out`, hexdump stdout. dasm은 실패하면 출력 파일을 쓰지 않고 기존 파일도 지웁니다.

## 6. 알려진 한계

`document/todo.md` "1. 어셈블러 / 링커"에 할 일로 있습니다. 여기서는 코드를 읽을 때 알아야 할 것만:

- 4바이트보다 큰 allocate의 비상수 값은 첫 워드만 패치, 크기가 4의 배수가 아니면 뒤 데이터가 정렬되지 않음.
- 중첩 `#include`의 경로 기준이 항상 최초 입력 파일 디렉터리(`PreprocData::fileName`이 갱신되지 않음).
- `gen[n]` 인덱스 범위 검사 없음(25 이상이면 특수 레지스터 id와 겹침).
- global/extern 오류 메시지의 토큰이 이름이 아니라 키워드로 표시됨.
- LALR 테이블 충돌이 조용히 묻혀서, 일부 잘못된 소스(`set gen[1].32B,` 뒤에 식 없음 등)가 엉뚱한 위치에서 실패함.
- `jp`/`jpi`/`break` 별칭은 넣지 않음(라벨 이름과 충돌 위험). 캐시 md 비트 방향(flush=1)은 가정.

## 7. 작업 주의사항

- 사용자가 `assembler.hpp`/`linker.hpp`를 대화 밖에서 동시에 고치는 경우가 많았습니다. 편집 직전에 다시 읽습니다. 사용자가 써 둔 스켈레톤(필드·함수 이름)은 그대로 따르고, 타입·이름을 바꿔야 하면 먼저 말합니다.
- 백슬래시가 든 스크립트는 bash heredoc/sed 대신 파일로 만들어 실행합니다(이스케이프가 한 단계 사라진 적이 있음).
- 빌드와 회귀 확인은 묻지 않고 해도 됩니다. 배포(`bin/` 교체)와 커밋은 요청이 있을 때만.
- 문법·인코딩·옵션을 바꾸면 `document/SYNTAX.md`도 같이 고칩니다. ISA 시트(`document/ISA-32.xlsx` → `ISA.md`)가 opcode·필드의 기준입니다.
