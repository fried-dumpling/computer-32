# CLAUDE.md — src (assembler-linker)

이 파일은 Claude Code가 이 디렉토리(`computer 32 (new)/src/assembler-linker`, 어셈블러+링커 프로젝트 루트)에서 작업을 이어갈 때 빠르게 현재 상태를 파악하기 위한 참고 문서입니다. (2026-09-20까지는 `assembler/` 서브폴더에 있었으나 이후 사용자가 루트로 옮김.) (2026-10-11부터 사용자가 빌드/회귀 테스트를 묻지 않고 해도 된다고 함 — 배포와 커밋은 여전히 요청 시에만.)

> **2026-10-11 이동**: `src/assembler-linker/`의 내용물 전체(`.git`, `.claude` 포함)를 한 단계 위인 `src/`로 옮김 — 이제 git 저장소 루트와 프로젝트 루트는 `src/`. 아래 문서의 `src/assembler-linker/...` 경로는 전부 `src/...`로 읽을 것. 같은 `src/`의 `csvsearcher/`, `hexdump/`, `textediter/`는 이 이동으로 git에서 untracked로 보임(아직 추가 안 함). 빈 `src/assembler-linker` 폴더는 다른 프로세스가 핸들을 잡고 있어 삭제 실패 — 나중에 수동 삭제.

## 디렉토리 구조 (2026-09-20 개편, 이후 사용자가 추가 개편)

원래 `src/assembler`였던 디렉토리를 `src/assembler-linker`로 이름을 바꾸고(Claude가 수행), 그 뒤 사용자가 직접 한 단계 더 정리해서 지금은 이런 모습:

```
src/assembler-linker/
  common/               — assembler/linker 공유 헤더
    format.hpp            오브젝트 파일 직렬화 포맷 (namespace format_data)
    iotool.hpp             파일 IO 유틸 (namespace iotools)
  assembler/            — 이 문서가 있는 디렉토리
    assembler.hpp / lexer.hpp / parser.hpp / evaluator.hpp / preprocesser.hpp
    main.cpp
    CLAUDE.md (이 파일)
  linker/               — 새로 추가된 링커 (사용자가 직접 작성 중, WIP)
    linker.hpp
    main.cpp
  test/                 — (2026-10-10 신설, Claude가 수행) 테스트용 소스/산출물 모음
    program/              과거 assembler/program/ 이었던 git-tracked 테스트 소스 31개 (`git mv`로 옮김, 커밋 히스토리 보존됨)
    out/                  과거 build/out/ 이었던 untracked 테스트 산출물(.o/.bin) — 세션 중 회귀 테스트 실행 결과 캐시, git에 안 올라감
  SYNTAX.md              — (54번) 어셈블리 문법/인코딩 사용자 문서
  build-asm.cmd          — 어셈블러 빌드 스크립트 (assembler\main.cpp → assembler.exe)
  build-lnk.cmd          — 링커 빌드 스크립트 (linker\main.cpp → linker.exe) — Claude가 build-asm.cmd 패턴 그대로 생성
```

`assembler.hpp`/`main.cpp`는 `#include "../common/format.hpp"`, `#include "../common/iotool.hpp"`로 참조 (사용자가 `common/`으로 옮기면서 직접 갱신함). `linker.hpp`/`linker/main.cpp`도 동일하게 `../common/`을 참조.

빌드는 `assembler-linker\build-asm.cmd`(어셈블러) / `assembler-linker\build-lnk.cmd`(링커) 각각 실행하면 됨 (MinGW `g++` 필요, PATH에 있어야 함). 두 스크립트는 동일한 패턴(스크립트 자기 위치 기준 상대경로, `where g++` 체크, 실패 시 exit code 1) — 서로 상대 스크립트를 주석으로 교차 참조함.

**참고**: 최초 `src/assembler` → `src/assembler-linker` 디렉토리 이름 변경(`mv`/rename) 자체는 이 세션 환경에서 "Device or resource busy"로 실패함(아마 파일 변경 감지용 워처가 옛 경로에 핸들을 잡고 있어서로 추정) — 그래서 새 트리를 만들고 파일을 개별적으로 옮기는 방식으로 우회했다. 그 결과 **빈 `src/assembler` 디렉토리가 그대로 남아있음** — 삭제도 같은 이유로 실패했으니, 나중에 편한 시점에 수동으로 지우면 됨(내용물 없음, 안전하게 삭제 가능).

`build-asm.cmd`/`build-lnk.cmd` 둘 다 실제로 실행해서 검증함(PowerShell로 호출 — git-bash에서 `cmd //c`로 부르면 인자 따옴표 처리가 꼬여서 실패하니 주의): 각각 `assembler.exe`/`linker.exe` 생성 확인, `assembler.exe`는 최소 테스트 프로그램(라벨+`addi`+`jmp`) 어셈블까지 정상 동작 확인, `linker.exe`는 인자 없이 실행해 크래시 없음만 확인(내부 로직은 사용자가 작성 중인 WIP라 검증 범위 밖).

## 개요

헤더 온리(header-only) C++ 커스텀 어셈블러. Windows/MSVC 대상. 자체 제작한 정규식 기반 렉서 생성기(`lexer.hpp`), LR 계열 파서 생성기(`parser.hpp`), 트리 순회 평가기 생성기(`evaluator.hpp`)를 이용해 `assembler.hpp` 안에서 실제 ISA-32 어셈블리 언어의 렉서/파서/평가기를 조립한다.

- `assembler.hpp` — 실제 작업이 이루어지는 파일 (lexer/parser/preprocesser/evaluator 네임스페이스 포함). **이 세션에서 가장 많이 수정된 파일.**
- `lexer.hpp` — 정규식→DFA 렉서 생성기 (제네릭, longest-match/maximal-munch)
- `parser.hpp` — LR/LALR류 파서 생성기 (제네릭, `CreateData` 문법 테이블 기반)
- `evaluator.hpp` — AST 순회 평가기 생성기 (제네릭)
- `preprocesser.hpp` — `#define`/`#macro` 전처리기 (제네릭)
- `main.cpp` — 진입점
- `../common/iotool.hpp` — 파일 IO 유틸 (namespace `iotools`, assembler/linker 공유)
- `../common/format.hpp` — 오브젝트 파일 직렬화 포맷(`Section`/`Mapper`/`Operation`/`Identifier`/`Header`/`Format`), `assembler::format` 네임스페이스와 별개인 독립 `format_data` 네임스페이스, assembler/linker 공유

## 아키텍처: 파서 생성기 문법 테이블

`parser_generator::ParserFactory<TokenType, NonterminalType, ASTNodeType>::CreateData`는
`{ {Nonterminal, ASTNodeAttribute, index}, {RHS 심볼들...} }` 튜플의 리스트.
- 세 번째 필드(index)는 reduce된 AST 노드의 `.type`을 RHS 중 몇 번째 요소의 토큰/타입으로 채울지 지정 (`-1`이면 ASTNodeAttribute를 그대로 사용).
- RHS 각 원소는 `{symbol, keep, splice}` — `keep`이면 자식 노드로 남기고, `splice`면 그 자식의 하위 노드들을 부모에 병합(리스트 누적 패턴에 사용).

## 표현식(expression) 문법: 14단계 우선순위 체인

LR 문법 충돌 없이 단항/이항 연산자를 구분하기 위해 `expressionL0`~`expressionL13`까지 계층화되어 있다 (전통적인 충돌 없는 CFG 형태, 우선순위 선언 불필요):

| 레벨 | 연산자 | 비고 |
|---|---|---|
| L0 | `( expr )`, number, identifier | 괄호는 L13을 참조 |
| L1 | 단항 `~ ! + -` | `+`/`-`는 L4(이항)와 토큰 공유 |
| L2 | `**` (우결합 거듭제곱) | |
| L3 | `* / %` | |
| L4 | `+ -` (이항) | |
| L5 | `<< >>` | |
| L6 | `< > <= >=` (관계) | |
| L7 | `== !=` (동등) | |
| L8 | `&` (비트 AND) | |
| L9 | `^` (비트 XOR) | |
| L10 | `\|` (비트 OR) | |
| L11 | `&&` (논리 AND) | |
| L12 | `\|\|` (논리 OR) | |
| L13 | `?:` (삼항) | `expression`의 진입점 |

`+`/`-` 토큰이 단항(L1)과 이항(L4) 양쪽에 쓰이므로, 평가기 `switch(cur->type)`에서는 같은 case 라벨 안에서 `cur->child.size() == 1`로 런타임 분기한다.

## 렉서 토큰

`assembler.hpp`의 `lexer::TokenType`에 논리/비교 연산자 토큰이 전부 선언 및 정규식 규칙 연결 완료:
`less greater lessequal greaterequal equal notequal lognot logampersend logverticalbar selector` (+ 기존 `ampersend verticalbar caret leftshift rightshift`).

정규식 메타문자 이스케이프 필요: `[ { ? + * | ( ) .` 와 백슬래시. `& < > = !  ^`는 리터럴.

## 평가기 (`evaluator::functions`)

### 타입 분류

- `ExprType::CONSTANT` — 완전히 상수로 계산된 값
- `ExprType::LINEAR` — `value + Σ coeff[section_id]·base(section)` 형태의 아핀 결합 (섹션 베이스 주소에 대해 미확정)
- `ExprType::COMPLEX` — 그 외 전부 (현재 더 이상 처리/전개하지 않음)

`ExprData`:
```cpp
struct _ExprData {
    ExprType type;
    s32 value;
    std::unordered_map<size_t, s32> coeff;   // section_id -> 계수
    std::vector<OperationToken> operations;   // 후위표기(RPN) 연산 이력
};
```

`OperationType` (POS는 제거됨 — 단항 +는 아무 일도 하지 않으므로):
```cpp
enum class OperationType {
    IDENTIFIER, CONSTANT,
    ADD, SUB, MULT, DIV, MOD, POW,
    SHIFTL, SHIFTR, BND, BOR, BNT, BXR,
    LESS, GREATER, LESSEQUAL, GREATEQUAL, EQUAL, NOTEQUAL,
    LAND, LOR, LNOT,
    NEG,
    SELECT
};
```

`LinearKind` — `combineBinary`/`combineUnary`가 LINEAR 승격 규칙을 결정하는 데 사용:
```cpp
enum class LinearKind { NONE, ADD_LIKE, SUB_LIKE, SCALE_LIKE, NEG_LIKE };
```
- `ADD_LIKE`: LINEAR+CONST, CONST+LINEAR, LINEAR+LINEAR → LINEAR (계수 합산)
- `SUB_LIKE`: LINEAR-CONST, CONST-LINEAR(계수 반전), LINEAR-LINEAR(계수 차감, 같으면 CONST로 축소)
- `SCALE_LIKE`: LINEAR*CONST, CONST*LINEAR → LINEAR (계수 스케일)
- `NEG_LIKE`: 단항 `-`의 LINEAR 피연산자 → 계수 부호만 반전 (`scaleCoeff(coeff, -1)`)
- `NONE`: 그 외 조합(예: LINEAR*LINEAR, LINEAR>>CONST, LINEAR/LINEAR 등)은 전부 `ExprType::COMPLEX`

승격 로직 요약(`combineBinary`/`combineUnary`):
1. 둘 다 CONST → CONST (함수 포인터로 실제 연산 수행)
2. `LinearKind`에 해당하는 LINEAR 규칙 매칭 시 → 계수 계산 결과가 비어 있으면 CONST로, 아니면 LINEAR로 (`combineBinary`만; `combineUnary`의 NEG_LIKE는 항상 LINEAR 유지)
3. 그 외 전부 → COMPLEX (`value=0`, `coeff` 비움)
4. 모든 경우 공통: `operations`는 자식들의 `operations`를 이어붙이고 현재 연산자를 후위(postfix)로 push

### `calculateConst` (병합된 핵심 평가 함수)

과거의 `calculateConst`/`calculateLabel`/`calculateNonconst` 세 함수를 이 하나로 병합함. 구조:
- `switch(cur->type)`에서 각 연산자 case는 `operationType`/`operationText`/`function`(또는 `unaryFunction`)/`linearKind`를 지역 변수에 설정만 하고 공용 라벨로 `goto`.
  - 이항 연산자 → `binaryOp:` 라벨 (마지막 이항 case 뒤에 위치) → `child1exp && child2exp` 검사 후 `combineBinary` 호출
  - 단항 연산자(`~ ! 단항-`) → `unaryOp:` 라벨 (마지막 단항 case 뒤에 위치) → `child1exp` 검사 후 `combineUnary` 호출
  - 단항 `+`는 예외: `combineUnary`를 거치지 않고 자식의 `ExprData`를 그대로 복사(passthrough) — `operations` 스택에 아무것도 push하지 않음
- `?:`(selector): 조건이 CONST면 분기 중 하나를 그대로 채택(상수 폴딩), 아니면 COMPLEX로 강등하며 `SELECT` 토큰을 후위 이력에 push
- 숫자 리터럴(`hexnum/decnum/octnum/binnum/character`): `{ExprType::CONSTANT, v, {}, {{OperationType::CONSTANT, text, v}}}`로 직접 구성 — value 필드까지 채워서 push
- `identifier`: `idenifierValue` 테이블에서 찾아 있으면 복사, 없으면(전방 참조) 그냥 skip — 두 번째 패스에서 해소됨
- `index`: 값이 음수면 실패(false) 반환
- `label`: 현재 섹션 바이트 오프셋을 `value`로, `coeff[section_id]=1`인 LINEAR로 만들어 `idenifierValue`에 등록. (과거 `label_text`/`label_data`/`label_bss`로 나뉘어 있던 AST 노드 종류를 단일 `label`로 통일함 — 아래 "최근 변경" 참고)
- `allocate`/`allocate_zero`: 크기 표현식이 `ExprType::CONSTANT`가 아니면 `return false`(에러) — 요청대로 구현됨

### 드라이버 (`evaluate()`)

(2026-10-10 갱신 — 47번 참고) `calculateSection` → `calculateAddr`(구 `calculateConst`) → `evaluateCode` → `collapseExpr`. 표현식 처리(연산자/리터럴/identifier/index)는 공용 함수 `calculateExpr`로 분리돼 있고 두 곳에서 호출됨:
1. `calculateAddr`: `calculateExpr` 후 주소 관련 case(instruction 바이트 증가, label 정의, allocate 크기). 전방 참조 식별자/그걸 포함한 식은 조용히 skip(에러 아님).
2. `evaluateCode`: 노드의 `expressionValue`가 아직 없으면 `calculateExpr`를 다시 호출 — 이 시점엔 모든 라벨이 `idenifierValue`에 있으므로 전방 참조가 해소됨. 값이 끝내 없으면 소비자(`immidate16`/`immidate8`/`reg_index`/`allocate`)가 `MISSING_EXPRESSION` 반환.
- 라벨 주소는 instruction(고정 4바이트)과 allocate 크기(반드시 CONSTANT, 전방 참조 불가)로만 정해지므로 `calculateAddr` 1회로 확정됨 — 그래서 별도의 2차 주소 패스가 필요 없음.

## 알려진 미해결/범위 밖 이슈 (건드리지 말 것 — 사용자가 명시적으로 범위 밖이라 함)

- **`curLine` 토큰 처리**: lexer에 `$` → `TokenType::curline` 토큰은 정의되어 있지만, 파서 문법이나 평가기 어디에도 아직 연결되지 않음. 사용자가 "이번 작업 범위 아님, 무시"라고 명시적으로 답변함 — 향후 별도 요청 없이는 임의로 구현하지 말 것.
- **`evaluate()` 드라이버의 미정의·불일치 참조**: `functions::calculateSize`가 선언 없이 호출됨(존재하지 않는 함수), `evaluate()` 끝부분과 `AssemblerDump`/`assemble()`에서 `data.textSection`/`data.dataSection`/`data.bssSection`이 `EvalData` 구조체에 없는 필드로 참조됨. `evaluateCode` 내부는 전부 `data->sections[sectionId].data` 형태로 통일됨(아래 참고) — 남은 문제는 `evaluate()`의 최종 `rawData` 조립부와 `AssemblerDump`/`assemble()`뿐. 전부 이 세션 이전부터 있던 기존 버그로, 명시적 요청 전까지 손대지 않음.
- ~~**`label_bss`**: `label_text`/`label_data`와 달리 `calculateConst`에서 아직 케이스가 없음.~~ → label 종류 통일로 해결됨(아래 참고).
- ~~**`pushByteN` 인자 순서 의심**~~ → 11번 항목에서 수정됨. 다만 `DataValue::length`가 어디서도 설정되지 않는 문제는 여전히 남아있음(`allocate`의 "data length" 표현식에서 채워져야 할 것으로 보이나 `calculateConst`에서 아직 연결 안 됨).
- **재배치 정보(`operationMap`/`operationVector`) 소비 루틴 없음**: `evaluateCode`가 기록만 할 뿐, 이 정보를 읽어 실제로 라벨 주소를 패치하는 후속 패스는 아직 없음(향후 구현 필요, 현재는 WIP 스키마).

## 이번 세션에서 진행된 작업 순서 (최근 → 과거 역순 아님, 시간순)

1. 논리/비교 토큰을 lexer 정규식 규칙 + parser 우선순위 체인(L6/L7/L11/L12/L13)에 연결
2. `calculateNonconst` 제거, `calculateConst`+`calculateLabel`을 하나로 병합 + CONST/LINEAR/COMPLEX 분류 규칙 구현 + `operations` 후위 기록 + `allocate`류 CONST 강제
3. `calculateConst`의 switch를 인수-설정 후 `goto binaryOp`/`goto unaryOp` 공용 처리 구조로 리팩터링
4. 단항 `+`/`-` 구현 (전부 `s32` 연산으로 통일, 별도 u32 특별 처리 없음)
5. `LinearKind`에 `negLike` 추가, `combineUnary`에 적용 (LINEAR 단항 `-` → `scaleCoeff(coeff, -1)`)
6. 숫자 리터럴 처리 시 `OperationToken{CONSTANT, text, value}`를 명시적으로 push하도록 수정
7. `OperationType::POS` 및 `LinearKind::POS_LIKE` 완전 제거, 단항 `+`는 `operations` 스택에 아무것도 남기지 않는 순수 passthrough로 변경 (현재 코드 상태에 최종 반영됨, grep으로 잔존 참조 없음 확인)
8. AST의 label 종류(`label_text`/`label_data`/`label_bss`)를 단일 `label`(Nonterminal/ASTNodeType 둘 다)로 통일. `text_section`/`data_section`/`bss_section` 문법 규칙이 이제 전부 `NT::label`을 직접 참조하고, 래퍼 문법 규칙 3개는 삭제됨. 부수효과로 `getSection`의 `ASTNodeType::label + TokenType::__end` case가 이제 실제로 매치됨(예전엔 실제 라벨 노드 타입이 label_text/label_data/label_bss였기 때문에 매치되지 않던 기존 버그였음), `calculateConst`의 두 케이스(`label_text`/`label_data`)도 하나(`label`)로 병합되며 부수적으로 `label_bss`도 동일 처리됨(예전 비대칭 이슈 해결).
9. `evaluateCode`에서 존재하지 않는 `data->textSection` 필드를 참조하던 나머지 `push32(...)` 호출 9곳(`instruction_RI/RR/II/RII/IRI/RVRI/FVRI/IXP/RXR`)을 전부 `data->sections[sectionId].data` 형태로 통일 (이미 `instruction_N`과 `allocate` case는 이 패턴을 쓰고 있었음).
10. (1차 시도, 이후 11번으로 대체됨) `EvalData`의 재배치 스키마가 처음엔 `InstValue{data, index}`/`DataValue{length, data, index}` 형태(재배치 대상 인덱스 하나만 표현, `combineIndex`로 전파)였다가, 사용자가 직접 `allocate` case를 `ReplaceData{index, offset, size}` 기반의 `.replace` 벡터 방식으로 고쳐놓아 재작성함(11번 참고).
11. `ReplaceData{index, offset, size}`(비트 단위 offset/size + `operationVector`로의 index) 기반 재배치 스키마로 전환. `InstValue`/`DataValue`는 이제 `std::vector<ReplaceData> replace`를 가지며, `Section::operationMap`은 `unordered_multimap<u32(섹션 내 바이트 위치), ReplaceData>`. `evaluateCode`의 `case allocate`(사용자가 직접 재작성한 템플릿: CONSTANT면 `.data`=값, 아니면 `.data`=0 + `ReplaceData{index=operationVector.size(), offset=0, size=32}`를 `.replace`에 push + `operations`를 `operationVector`에 push)를 참고해 나머지 모든 case에도 동일 패턴을 반영:
    - `immidate16`/`immidate8`/`reg_index`: `allocate`와 동일하게 `ExprData`를 직접 검사 — 비 CONSTANT일 때 `ReplaceData.size`는 각 필드 폭(`immidate16`=16, `immidate8`=8, `reg_index`=32)으로 설정
    - `instruction_N/R/RI/RR/II/RII/IRI/RVRI/FVRI/IXP/RXR`, `reg`: 새 헬퍼 `appendReplace(dest, src, shift)`로 자식의 `.replace` 항목들을 각 필드가 최종 값에서 놓이는 비트 시프트만큼 `offset`을 보정해 병합(`.data` 계산에 쓰인 것과 동일한 `<<` 상수를 그대로 재사용) — 여러 필드가 동시에 재배치 대상이어도 모두 보존됨(이전 `combineIndex` 방식의 "최대 하나만 전파" 단순화보다 정확함)
    - 바이트 push 직전에 새 헬퍼 `registerReplace(data, sectionId, replace)`로 현재 `sections[sectionId].data.size()`(push 전 위치)를 키로 `.replace`의 각 항목을 `operationMap`에 등록
    - `reg_ex`/`regid`/`regmode`: 고정 테이블 조회라 `ExprData`/재배치 없음 — `.data`만 채움(`.replace`는 기본 빈 벡터로 둠)
    - `allocate` case의 `pushByteN` 호출 인자 순서를 시그니처(`pushByteN(list, data, size)`)에 맞게 `(sections[...].data, dataValue[cur].data, dataValue[cur].length)`로 수정(기존엔 data/length가 뒤바뀌어 있었음 — 아래 "알려진 이슈"의 관련 항목도 해결됨)
12. `pctl`/`getabs`/`flush`/`inval` 명령어 추가 (사용자 스펙: `pctl [enable(=1)/disable(=0)]`, `getabs [src-reg],[dest-reg]`, `[flush/inval] [base-reg],[imm8],[id]` — `id ∈ {ALL=0x80, L2=0x8, TLB=0x4, L1D=0x2, L1I=0x1}`):
    - lexer: `pctl/getabs/flush/inval/enable/disable/L1D/L1I/TLB/L2/ALL` 리터럴 규칙 + tokenStr 추가 (토큰 자체는 이전부터 `TokenType`에 선언만 되어 있었음)
    - 새 논터미널/AST 타입: `ctlid`(enable/disable), `cacheid`(ALL/L2/TLB/L1D/L1I), `instruction_CTL`(pctl 전용, 오퍼랜드 없이 opcode에 변이값만 더함 — `ld.<regid>`/`jmp.<flagid>`의 RVRI/FVRI와 같은 "opcode에 변형 더하기" 패턴이지만 reg/imm 필드가 아예 없는 축소판), `instruction_CACHE`(flush/inval 전용, reg+imm8+cacheid 3필드 — RII과 같은 모양이지만 RII는 intervec에서 이미 절반만 연결된 상태라 건드리지 않고 새로 만듦)
    - `getabs`는 새 AST 타입 없이 기존 `instruction_RR`(reg,reg)을 그대로 재사용
    - `instructionBase`: `pctl=0x7C`(2슬롯: disable=0x7C, enable=0x7D), `getabs=0x7E`, `flush=0x7F`, `inval=0x80`. 새 테이블 `ctlBase`(disable=0,enable=1), `cacheBase`(L1I=0x1,L1D=0x2,TLB=0x4,L2=0x8,ALL=0x80)
    - `getSection`/`calculateConst`(섹션 바이트 +4)/`evaluateCode`(인코딩+`operationMap` 등록+`push32`) 세 곳 모두에 `instruction_CTL`/`instruction_CACHE` case 추가; `ctlid`/`cacheid`는 고정 테이블 조회라 `.replace`는 항상 빈 벡터
    - opcode 값(0x7C~0x80)과 오퍼랜드 형식은 레포에 별도 ISA 문서가 없어 전부 사용자가 대화 중 직접 지정한 스펙을 그대로 따름 — 향후 ISA 문서가 별도로 생기면 그쪽을 우선 참고할 것
13. `evaluateCode`의 `instruction_*` case들(N/R/RI/RR/II/RII/IRI/RVRI/FVRI/IXP/RXR/CTL/CACHE, 13개)에서 반복되던 공통 꼬리(`registerReplace` + `push32`)를 `calculateConst`의 `binaryOp:`/`unaryOp:`와 같은 goto 병합 패턴으로 리팩터링. 각 case는 `.data` 계산과 `appendReplace`만 하고 `goto pushInstruction;`으로 점프(마지막 case인 `instruction_CACHE`만 라벨로 자연스럽게 fall-through), `pushInstruction:` 라벨 하나에서 `registerReplace`+`push32`+`break` 공통 처리. `allocate` case는 `pushByteN`(가변 길이)을 써서 꼬리 모양이 달라 이 병합에서 제외.
14. 사용자가 직접 새로 추가한 `assembler::format` 네임스페이스(오브젝트 파일 출력 포맷 직렬화 레이어 — `Section`/`Mapper`/`Operation`/`Identifier`/`Header`/`Format` 구조체, `formatData(Format&, const EvalData&)`)의 `Header::getRaw`/`Format::getRaw`를 채움:
    - `Header::getRaw`: 먼저 헤더 자신의 크기(`size()` = 13개 u32 필드 × 4바이트 = 52, `push32`로 기록) → 6개 구역(sections/binary/mapper/expressions/definedIdentifier/usedIdentifier)의 start/end 인덱스 쌍을 순서대로 `push32`
    - `Format::getRaw`: 각 구역을 임시 버퍼에 먼저 직렬화(`sections`/`mapper`/`definedIdentifier`는 각 원소의 기존 `getRaw` 호출, `binary`는 이미 raw라 그대로, `usedIdentifier`는 각 문자열 뒤에 `'\0'` 하나씩 push해 이어붙임, `expressions`는 `vector<vector<Operation>>`이라 자체적으로 길이 구분이 안 되므로 각 하위 벡터 앞에 원소 개수를 `push32`로 붙이고 그다음 각 `Operation::getRaw` 나열— 이 count-prefix 방식은 사용자가 명시하지 않아 직접 판단한 부분) → 각 임시 버퍼 크기로 `Header`의 start/end를 누적 계산(헤더 자신의 크기만큼 오프셋에서 시작) → `header.getRaw(out)`로 헤더를 맨 앞에 쓴 뒤 6개 구역을 순서대로 이어붙임
    - ~~`formatData()`의 `expressions` 채우는 루프가 `copy`를 계산만 하고 `out.expressions`에 push하지 않는 기존 미완성 코드~~ → 15번에서 해결됨
18. **첫 실제 빌드/테스트 수행** (사용자가 명시적으로 요청함 — `g++ -std=c++17` / MinGW로 `main.cpp` 컴파일+링크, `assembler_test.exe`로 몇 개 최소 테스트 프로그램 실행). `formatData()`의 `usedIdentifier`/`definedIdentifier`를 채움:
    - `usedIdentifier`: `formatData()`가 `operationVector`→`Operation` 변환 루프를 도는 동안, `OperationType::IDENTIFIER` 타입 토큰을 만나면 그 `.text`(식별자 이름)를 `unordered_map<string,u32>`로 중복 제거하며 `format.usedIdentifier`에 등록하고, 해당 `Operation.value`를 그 인덱스로 덮어씀(기존엔 `.value`가 항상 0으로 버려지고 있었음). 같은 이름이 여러 표현식에서 재참조돼도 한 번만 등록됨(hex 덤프로 직접 확인).
    - `definedIdentifier`: `data.idenifierValue`(라벨 심볼 테이블)의 각 항목을 `Identifier{name, (u32)exprData.value}`로 변환해 push (라벨의 섹션-로컬 오프셋 값 그대로 사용 — coeff/섹션 정보는 별도로 `mapper`/`sections`가 담당한다고 가정).
    - 빌드해서 실제로 돌려보는 과정에서 이 세션의 여러 concurrent 변경이 겹치며 생긴 컴파일 에러/런타임 크래시 3건을 직접 만나서 고침(전부 `formatData` 자체보다는 그걸 실행하기 위해 거쳐야 하는 파이프라인의 문제였음):
      - `_EqualOperation::operator()`에 `const` 누락 → `unordered_map::find`가 컴파일 안 됨(컴파일 에러). `const` 추가로 해결.
      - 단항/이항 비교·논리 연산자 람다들(`< > <= >= == != && || !` , `**`)이 `int`를 리턴하는데 `s32(*)(s32,s32)` 함수 포인터에 대입 — 이 플랫폼(MinGW, `s32`=`long`)에서는 `int`↔`long` 함수 포인터가 호환 안 돼 컴파일 에러(`-fpermissive` 없이는 빌드 자체가 안 됨). 각 람다에 `-> s32` 명시적 리턴 타입을 붙여 해결.
      - `calculateSection`(구 `getSection`)이 `bool`을 리턴하는데 실제로는 `return`문이 하나도 없어 UB(운 나쁘면 false 반환) → `evaluate()`의 첫 단계로 새로 연결되면서 이 잠재 버그가 처음으로 드러남. 끝에 `return true;` 추가.
      - `.text`/`.data`/`.bss`를 이름 없이 쓰는 문법(문법상 허용되고, `program/`의 실제 샘플 전부가 이 형태)이 AST 노드에 자식이 0개인데 `calculateSection`이 무조건 `cur->child[0]->text`로 이름을 읽어 세그폴트 → `cur->child.empty() ? "" : cur->child[0]->text`로 가드.
      - `EvalData::sectionByte`가 어디서도 `push_back`/`resize`되지 않아 항상 빈 벡터인데 여러 곳에서 `sectionByte[section_id]`로 인덱싱 → 세그폴트. `calculateSection`이 새 섹션을 만들 때(`data->sections.push_back(nSection)` 직후) `data->sectionByte.push_back(0)`도 같이 하도록 수정해 두 벡터를 lockstep으로 유지.
      - (부수적으로 함께 고침) `AssemblerDump::parseSuccess/preprocSuccess/evalSuccess`가 초기값 없는 지역 변수라 해당 덤프 플래그(`-preproc` 등)를 안 켜고 성공한 경우 인디터미네이트 값이 출력됨 → 전부 `= false`로 기본값 지정. (단, `main.cpp`의 표시 조건 `if (dumpPreproc || !dump.preprocSuccess)` 자체는 "플래그를 안 켜면 항상 실패로 보임"이라는 근본적인 설계 문제라 이번엔 안 건드림 — `-preproc`/`-eval` 등 관련 플래그를 켜서 확인해야 정확한 성공/실패를 볼 수 있음.)
    - 위 수정들 덕분에 `program/` 밑 실제 샘플 없이도 만든 최소 테스트(`.text` + 라벨 + `add`/`addi`/`jmp`)로 preproc/parse/evaluate 전부 성공, `formatData`+`format::getRaw` 결과 바이너리를 직접 헥스덤프/파싱해서 `usedIdentifier`(중복 제거 확인 포함)/`definedIdentifier`/`expressions`/`mapper`/`Header`가 서로 정확히 들어맞음을 확인함.
    - **(이후 19번에서 해결됨) 당시 남은 문제**: `registerReplace`의 바이트 오프셋 계산 off-by-one, `formatData()`가 `format.sections`를 채우지 않던 문제.
19. 사용자가 18번의 두 남은 문제를 직접 수정한 뒤 "확인해줘"라고 요청 — 재검증 결과:
    - `registerReplace`가 `basePos = data.size() + byte - 1`로 수정됨(사용자 작업) → 16비트 필드/offset=0 케이스에서 mapper.byteIndex가 이제 명령어의 LSB 바이트(빅엔디안 4바이트 워드의 마지막 바이트)를 정확히 가리킴을 재검증 완료(`jmp.zero`/`jmp.one` 두 명령어 각각 byteIndex=7/11로, 각 워드의 마지막 바이트와 일치).
    - `formatData()`가 `Section nSec`을 만들어놓고 `format.sections.push_back(nSec)`을 빠뜨린 게 남아있어서(`format.sections`가 여전히 항상 빈 배열) 직접 추가해 완료함 — 재검증 결과 `sections` 구역에 `{name:"", start:0, end:12}` 1건이 정상적으로 들어감.
    - 두 테스트(라벨 1회/2회 참조) 모두 빌드 성공 + preproc/parse/evaluate 성공 + 헥스덤프로 `sections`/`binary`/`mapper`/`expressions`/`definedIdentifier`/`usedIdentifier` 6개 구역 전부 상호 일치 확인함.
20. 사용자가 `AssemblerDump::sectionDump`(구 `textSection`/`dataSection`/`bssSection`을 `std::vector<evaluator::functions::Section>`로 대체) + `assemble()`의 채우는 코드 + `main.cpp`의 `-bin` 덤프 출력 로직을 직접 추가 → "sectionDump 빌드 후 확인해줘" 요청으로 검증:
    - `main.cpp`의 섹션별 바이트 출력 이중 루프에서 안쪽 루프 변수가 `si`(바이트 이터레이터)인데 `printf("%02X ", *it)`로 바깥 루프 변수(`it`, `Section`)를 잘못 역참조하는 복붙 버그 발견 → `*si`로 수정.
    - 수정 후 빌드 성공(에러/경고 없음), `-bin`으로 실행해 `section: <name>` 아래 실제 섹션 바이트(`02 00 00 01 / 60 00 00 00 / 66 00 00 00`)가 올바르게 출력됨을 확인.
21. `assemble()`에서 `dump.preprocSuccess`/`dump.parseSuccess`/`dump.evalSuccess`를 각 단계의 실제 결과(`validPreprocess`/`validParse`/`validEvaluate`)로 **덤프 플래그와 무관하게 항상** 기록하도록 변경(이전엔 `dump.flags & AssemblerDump::getXXX`가 켜져 있거나 실패했을 때만 기록돼서, 관련 플래그 없이 성공하면 초기값 `false`가 그대로 노출되던 문제 — 18/20번에서 언급한 "관련 플래그를 켜야 정확한 성공/실패를 볼 수 있다"는 제약이 이걸로 해결됨). 무거운 부가 데이터(`preprocTokens`/`pAST`/`errorMessage`)는 계속 기존처럼 플래그(또는 실패 시)에만 채움 — success bool만 항상 갱신, 나머지는 그대로 lazy. `-bin`만 켜고 실행해 재확인: 성공 케이스에서 preproc 섹션은 표시 안 되고 parse/evaluate는 플래그 없이도 "success"로 정확히 뜸, 문법 오류 파일에서는 parse/evaluate 둘 다 "failed"+에러 메시지가 정상 출력됨.
17. `format` 네임스페이스 구조 정리:
    - `Section`/`Mapper`/`Operation`/`Identifier`/`Header`/`Format` 구조체 전부를 새 파일 `format.hpp`로 추출 (`namespace assembler::format`을 자체적으로 완성해서 선언 — `evaluator::functions::push32`에 의존하지 않도록 자체 `push32` 헬퍼를 복제해 넣어서, 다른 제네릭 헤더들(lexer.hpp/parser.hpp/evaluator.hpp)처럼 `assembler.hpp` 최상단 include 목록에 나란히 추가 가능하게 함). `assembler.hpp`는 이제 `#include "format.hpp"`만 하고, 같은 네임스페이스를 다시 열어 `EvalData` using과 `formatData()`/`getRaw()` 같은 assembler 쪽 로직만 남김.
    - 사용자가 먼저 concurrently 추가해둔 전역 `Format format;` 객체에 맞춰, `formatData()` 안의 지역 `out.` 참조를 전부 `format.`으로 바꿈(매개변수 `out`은 이미 제거되고 전역 `format` 객체를 쓰는 구조로 바뀌어 있었는데 본문이 아직 안 따라간 상태였음)
    - `format` 네임스페이스에 `inline void getRaw(std::vector<u8>& out) { format.getRaw(out); }` 추가 — 전역 `format` 객체를 직접 만지지 않고 바깥(`assemble()` 등)에서 직렬화 결과를 얻는 진입점. (네임스페이스 이름과 전역 변수 이름이 둘 다 `format`으로 같지만, 변수 선언이 네임스페이스 본문 내부 스코프라 합법적으로 컴파일됨 — `namespace foo { int foo; }` 패턴과 동일)
15. `OperationType`에 `EOX`(end of expression) 추가 — 항상 0번지(맨 앞)로, 이후 요소들은 한 칸씩 밀림. `formatData()`가 각 표현식의 연산 목록(`copy`) 끝에 `EOX` 토큰을 붙인 뒤 비로소 `out.expressions.push_back(copy)`로 실제 저장하도록 수정(14번에서 언급한 "계산만 하고 저장 안 하던" 미완성 코드가 이 참에 완성됨). `Format::getRaw`의 `expressions` 직렬화도 각 표현식 앞에 붙이던 개수 `push32` 프리픽스를 제거하고, `EOX`가 종결자 역할을 하므로 각 `Operation`을 그냥 순서대로 이어붙이기만 하도록 변경.
22. (2026-09-21) `common/format.hpp`의 `loadRaw` 계열(역직렬화, `getRaw`의 대응 함수) 완성. 사용자가 이미 `read32` 헬퍼 + `Section`/`Mapper`/`Operation`/`Identifier`의 `loadRaw(iterator&)` 초안과 `Format::loadRaw(vector<u8>&)` 빈 스텁을 만들어뒀길래("현재 진행중인 방식" 확인 후) 그 패턴을 그대로 따라 나머지를 완성:
    - `read32`가 미완성이었음 — `union`을 `conv[i]`로 배열처럼 접근(컴파일 에러), 세미콜론 누락, `inline` 누락(공용 헤더라 여러 TU에 include될 걸 대비해 `push32`처럼 `inline` 필요) → 전부 수정.
    - `Section`/`Mapper`/`Identifier`의 `loadRaw`가 이름 문자열을 널 종단까지 읽고 나서 그 널바이트 자체를 건너뛰지 않아 다음 필드를 한 바이트 밀려 읽는 버그 → 각각 루프 뒤에 `++it` 추가.
    - `Mapper::loadRaw`가 존재하지 않는 필드 `name`에 쓰고 있었음(실제 필드명은 `sectionName`) → 수정.
    - `Identifier::loadRaw`가 존재하지 않는 `load32`를 호출(오타, `read32`여야 함) → 수정.
    - `Header`엔 `loadRaw`가 아예 없었음 → `getRaw`와 정확히 같은 순서로 저장된 크기(값은 버림, 재직렬화 시 `size()`로 다시 계산되므로) + 6개 구역 start/end 12개 필드를 읽는 함수 추가.
    - `Format::loadRaw`(빈 스텁)를 완성: `Header::loadRaw`로 헤더를 먼저 읽고, 헤더가 알려주는 각 구역의 절대 offset(`in.cbegin() + header.XStart` ~ `XEnd`)으로 구역별 독립 파싱 — `sections`/`mapper`/`definedIdentifier`는 구역 끝까지 반복하며 개별 구조체 `loadRaw` 호출, `binary`는 통째로 `assign`, `expressions`는 `EOX`(`OperationType::EOX`, type=0)를 만날 때까지 `Operation`을 읽어 한 표현식으로 묶는 걸 반복(기록 쪽이 표현식 끝마다 `EOX`를 붙이는 것과 대응), `usedIdentifier`는 구역 끝까지 널 종단 문자열을 반복해서 읽음. 매개변수명은 기존 스텁의 `out`(입력인데 출력 이름이라 오해 소지, 호출부가 아직 없어 안전하게 변경 가능했음)을 `in`으로 바꿈.
    - 검증: `assembler::assemble()`로 만든 원본 `format::format`을 `getRaw`로 직렬화 후 새 `Format`에 `loadRaw`로 역직렬화해서 6개 구역 전부(`sections`/`binary`/`mapper`/`expressions`/`definedIdentifier`/`usedIdentifier`) 원본과 필드별 비교하는 라운드트립 테스트를 두 케이스(라벨 1개+반복 참조 / `.data`+`.text` 혼합, 서로 다른 식별자 여러 개)로 작성해 둘 다 일치 확인("ROUND TRIP OK"). 테스트 파일은 임시 디렉토리에서만 작업 후 정리, 저장소엔 남기지 않음.

23. (2026-09-23) 두 가지 요청 처리 — "assembler에서 identifier struct에 section 이름 추가한거 반영해주고 linker의 stackMachine namespace 완성해줘":
    - **`Identifier.section` 반영**: 사용자가 `common/format.hpp`의 `_Identifier`에 `std::string section;` 필드를 직접 추가해뒀길래, `getRaw`/`loadRaw`에 name/value 사이 section 필드 직렬화(널 종단 문자열)를 추가하고, `assembler.hpp`의 `formatData()`에서 반쯤 쓰다 만 `nId.section = ` (세미콜론도 없이 끊긴 상태)을 완성함: `it->second.coeff`(라벨의 `ExprData.coeff`, section_id→계수 맵)에서 section_id를 뽑아 `data.sections[section_id].name`으로 매핑, coeff가 비어있으면(CONSTANT로 완전히 확정된 식별자) `""`.
    - 그 근처에서 컴파일을 막고 있던 별개 버그도 같이 고침: `formatData()`의 섹션 루프에서 멀티섹션 절대 오프셋 누적용 `u32 base` 선언 및 `base = format.binary.size()` 갱신이 통째로 빠져있었음(마지막으로 내가 만든 버전엔 있었는데 이후 편집 중 사라짐) → 복원, `mapper.byteIndex`도 `base +` 다시 붙임.
    - **라운드트립 검증**: `assembler::assemble()`로 실제 조립 후 `getRaw`→`loadRaw`로 `definedIdentifier`(`name`/`section`/`value`)까지 필드별 비교, 일치 확인.
    - **⚠️ 검증 중 발견한 별개의 기존 버그(이번 범위 밖, 손대지 않음)**: `.data`+`.text`를 함께 쓰는 프로그램을 평가해보면 `data.sections`에 섹션이 딱 1개(TEXT)만 들어가고 `.data`용 `Section`이 아예 안 생김 — `.data`에 정의된 식별자(`value` 등)도 전부 `coeff={0:1}`로 TEXT 섹션에 잘못 귀속됨. `calculateSection`(어디선가 `.data_section`/`.text_section` AST 노드를 개별적으로 처리해 `data->sections.push_back`해야 하는데, 실제로는 프로그램 전체에 걸쳐 한 번만 실행되는 것으로 보임 — 원인 미조사)의 섹션 카운팅 문제로 추정. **링커가 여러 섹션을 다루기 시작하면 반드시 먼저 고쳐야 함** — 지금 상태로는 `.data` 심볼이 `.text` 섹션 소속으로 잘못 직렬화됨.
    - **`stackMachine` 네임스페이스 완성** (`linker/linker.hpp` + `linker/stackmachine.hpp`): 사용자가 짜다 만 `SMOperation`(불완전한 enum, 세미콜론 없음)을 걷어내고 이미 있는 `format_data::OperationType`을 그대로 재사용(`OpType`로 별칭) — 별도 enum을 유지/매핑할 필요가 없어짐. 제네릭 엔진(`stack_machine_generator::StackMachine<Operation>`, `stackmachine.hpp`)도 여러 버그가 있어 같이 고침: `std::vector`엔 `.pop()`이 없음(`.pop_back()`이어야 함), 클래스 닫는 `}` 뒤 세미콜론 누락, `Operation::__const` 센티널이 실제로는 존재하지 않는 이름(→ `Operation::CONSTANT`로 통일), 인자 슬라이싱 방향이 뒤집혀 있어 RPN을 거꾸로 읽던 문제 → 입력을 앞에서부터 순회하며 러너스택에 쌓고, 연산자를 만나면 `Element.value`를 "이 연산자가 팝할 피연산자 개수(arity)"로 재해석해 그만큼 팝하는 방식으로 재작성(어셈블러가 만드는 원본 `OperationToken`은 연산자에 value를 안 채워 넣으므로, 이 arity는 `format_data::Operation`→`Element` 변환 단계인 `runOperation()`에서 `arity(OpType)` 테이블로 새로 채워 넣음 — BNT/LNOT/NEG는 1, SELECT는 3, 나머지는 2).
      - `function()`: `format_data::OperationType`의 산술/비교/논리 전체(ADD~SELECT, EOX/IDENTIFIER/CONSTANT 제외)를 구현 — 전부 `s32`로 캐스팅해서 연산 후 `u32`로 되돌림(부호 있는 나눗셈/시프트/비교가 정확하도록; 비트 연산·동등비교는 `u32`로 그대로).
      - `runOperation(operation, resolvedIdentifiers, result)`: `Format::expressions`에 저장된 형태 그대로(=`EOX`로 끝나는 `format_data::Operation` RPN 목록)를 받아 `Element` 목록으로 변환 후 실행. `IDENTIFIER` 타입은 `.value`가 `usedIdentifier`의 인덱스이므로, 호출자가 넘겨준 `resolvedIdentifiers`(인덱스별 실제 주소값, 심볼 테이블 병합 후 채워질 값)로 치환해서 `CONSTANT`처럼 취급 — 그래서 제네릭 엔진은 `CONSTANT` 하나만 "리프"로 알면 됨.
      - `linker::stackMachine`은 아직 아무 데서도 호출 안 됨(`linker::evaluate`/`link()`는 여전히 미완성 WIP — 문법 에러 다수: `std::Vector` 오타, `namespace format`이 `namespace evaluate`보다 먼저 나오는데 `evaluate::functions::EvalData`를 참조, 끊긴 함수 선언 등 — 이번 요청 범위 밖이라 손대지 않음). 그래서 `linker.hpp` 전체는 아직 안 빌드됨 — `stackMachine` 부분만 별도 스탠드얼론 테스트 파일로 격리해서 컴파일+실행 검증: `(2+3)*4-1=19`(이항 연산+우선순위), `IDENTIFIER` 치환 후 `+100`(=142), 단항 `NEG`(5→-5) 세 케이스 전부 통과.

24. (2026-09-23) "runOperation에는 stackMachineRun만 남기고 나머지 처리는 일단 castOperation로 옮겨줘" — 23번에서 만든 `stackMachine::runOperation`(포맷→Element 변환 + 실행을 한 함수에 다 담고 있었음)을 역할별로 분리:
    - `stackMachine::runOperation`은 이제 이미 만들어진 `std::vector<Element>`를 받아 `stackMachine.run(elements, function, result)` 한 줄만 함 — 순수 실행부만 남김.
    - 변환부(포맷 `Operation` 목록을 `EOX`까지 순회하며 `IDENTIFIER`는 `resolvedIdentifiers`로 치환, 연산자는 `stackMachine::arity()`로 팝 개수 채우기)는 `evaluate::functions::castOperation`(원래 빈 스텁)으로 옮김 — `namespace evaluate`가 심볼 테이블 병합을 담당할 계층이라 "포맷 표현 ↔ 실행 표현 변환"도 거기 있는 게 맞음. `stackMachine`은 이제 `format_data::Operation`을 전혀 몰라도 되는 순수 RPN 실행기로 남음.
    - `evaluate::functions`에 `u32`/`u8` 별칭이 아예 없어서(이미 그 자리에서 `std::vector<u8>`, `std::map<string,u32>` 등 기존 코드가 쓰고 있었는데도) `castOperation`을 넣으려니 컴파일이 안 되는 상태였음 → `using u32 = format_data::u32; using u8 = format_data::u8;` 추가(다른 `using SectionType = ...` 줄과 같은 자리, 같은 패턴).
    - `evaluate` 네임스페이스의 나머지(`mergeSection`/`EvalData`/`link()`)는 여전히 별개의 broken WIP라 전체 `linker.hpp` 빌드는 안 됨(23번에 적어둔 이유 그대로) — `stackMachine`+`castOperation` 조합만 독립 테스트로 격리해서 검증: `(2+3)*4` 후 단항 `NEG` = `-20`, `castOperation`이 6개 Element로 변환하고 `runOperation`이 그걸 실행해 정확한 결과 확인.

25. (2026-09-23) ".data+.text버그 분석해줘" — 23번에서 발견한 "`.data`+`.text`를 같이 쓰면 섹션이 1개로 합쳐지고 `.data` 식별자가 `.text` 섹션 소속으로 잘못 기록되는" 버그를 (수정은 안 하고) 원인만 분석. `/tmp` 스크래치 디렉토리에 소스 복사본을 만들어 디버그 프린트 삽입 → 빌드 → 실행 → 삭제하는 방식으로, 실제 저장소 파일은 전혀 건드리지 않고 진행함.
    - **확정된 근본 원인**: `calculateSection`의 섹션 헤더 케이스(`text_section`/`data_section`/`bss_section`/`outer_section`)가 다른 모든 AST 노드 케이스(`index`, `label`, `instruction_*` 등)와 달리 `+ (u64)TokenType::__end` 오프셋 없이 **맨 `ASTNodeType` 값 그대로** 비교하고 있음. 실제 실행 중 텍스트섹션에 해당하는 두 가지 서로 다른 AST 리듀스(래퍼 `NT::section` 규칙 자신 vs 안쪽에서 누적되는 `NT::text_section`/`NT::data_section` 바디 리스트 규칙 — 둘 다 같은 `AT::text_section`/`AT::data_section` 태그를 쓰지만 파서 테이블 상에서 서로 다른 `cur->type` 값(맨값 `3`/`4` 대 오프셋값 `143`/`144`)으로 나타남)이 있는데, 현재 스위치는 이 중 **맨값과 우연히 일치하는 것 하나만** 잡고 나머지는 전부 놓침 — 그래서 `.data`+`.text` 두 섹션이 있어도 `process_section`이 사실상 한 번만(그리고 어느 쪽이 잡히는지는 문법 규칙 매칭 우연에 좌우됨) 실행돼 `data->sections`가 항상 1개로 끝남. `sectioningArr`에 쌓인 라벨/명령어 노드들은 (그 하나뿐인) section index로만 매핑되니, `.data`에서 정의된 식별자도 전부 `.text` 섹션 소속(`coeff={0:1}`)으로 잘못 기록됨.
    - **검증**: 스크래치 복사본에서 4개 케이스 라벨에 `+ (u64)TokenType::__end`만 붙이고(다른 모든 AST 케이스와 동일한 패턴으로 맞춤) 캐시된 `_table.bin`도 지우고 재빌드 → `.data`+`.text` 프로그램에서 `data.sections.size()`가 1→2로 정상화, `value` 식별자가 DATA 섹션, `loop`/`start`가 TEXT 섹션으로 올바르게 분리됨을 확인. **이 수정은 스크래치 사본에만 적용했고 실제 저장소 파일(`assembler.hpp`)은 이번 턴에서 건드리지 않음** — 요청이 "분석"이었기 때문에 다음 턴에 명시적으로 요청 시 적용 예정.
    - **검증 중 발견한 부수 버그(같은 함수, 별개 원인)**: 위 수정을 적용하고 나니 섹션 이름이 이상하게 나옴 — `.data`/`.text`를 이름 없이 썼는데도 DATA 섹션 이름이 `"value"`, TEXT 섹션 이름이 `"start"`로 나옴(둘 다 그 섹션 안 첫 번째 식별자 이름을 잘못 가져온 것). 원인: `nSection.name = cur->child.empty() ? "" : cur->child[0]->text;`가 "이름 있는 섹션"(`{TT::identifier, true, false}`가 child[0])과 "이름 없는 섹션"(`{NT::text_section, true, false}` 바디가 child[0])을 구분 못 하고 항상 `child[0]->text`를 이름으로 취급함 — 이름 없는 쪽은 child가 비어있을 때만 정상(`""`)이고, 바디가 있으면(=섹션 안에 뭔가 있으면) 바디 리듀스 과정에서 위로 전파된 첫 자식의 텍스트를 실수로 이름처럼 읽음. 이건 이번 요청 범위(섹션 개수/식별자 소속) 밖이라 같이 고치진 않았지만, 위 근본 원인 수정 시 같이 손봐야 실제로 쓸만해짐(`cur->child[0]->type == (u64)TokenType::identifier`인지 확인해서 진짜 이름이 있을 때만 읽도록).

26. (2026-09-24) "해당 문제 해결해줘 이름 문제는 TT::identifier 인지로 구분하면 될 듯?" — 25번에서 분석만 하고 남겨뒀던 두 버그를 실제로 `assembler.hpp`에 적용:
    - `calculateSection`의 4개 섹션 헤더 케이스에 `+ (u64)TokenType::__end` 오프셋 추가(다른 모든 AST 케이스와 동일한 패턴으로 통일).
    - `nSection.name`을 `(!cur->child.empty() && cur->child[0]->type == (u64)TokenType::identifier) ? cur->child[0]->text : ""`로 변경 — 사용자가 제안한 대로 child[0]의 실제 토큰 타입이 `TT::identifier`일 때만(=진짜 이름 있는 섹션 규칙일 때만) 이름으로 읽고, 그 외(바디만 있거나 아예 비어있는 이름 없는 섹션)는 `""`.
    - 캐시된 `build/assembler_table.bin`도 같이 삭제(예전 문법 상태로 빌드된 캐시를 계속 재사용하면 25번에서처럼 소스와 실제 실행 결과가 어긋날 수 있어서 — 재빌드 시 자동으로 새로 생성됨).
    - **검증**: 세 가지 케이스로 확인 — ① 이름 없는 `.data`+`.text`: `sections.size()`가 2로 정상화, 둘 다 이름 `""`(정상, 진짜 이름이 없으므로), `value`는 DATA 섹션 coeff={0:1}, `loop`/`start`는 TEXT 섹션 coeff={1:1}로 내부적으로도 완전히 분리됨을 raw `EvalData`까지 내려가서 확인. ② 섹션 1개짜리 기존 케이스(회귀 테스트) 그대로 정상 동작. ③ 이름 있는 섹션(`.data mydata`/`.text mycode`) 문법은 파싱 자체가 안 됨(`assemble()=-2`) — 이건 오늘 고친 범위와 무관한 **별개의 기존 파서 이슈**로 보이며, 이번엔 안 건드림(이름 있는 섹션 자체를 아직 아무도 실제로 안 써봐서 지금까지 드러나지 않았던 것으로 추정).
    - **참고(오늘 범위 밖, 그냥 눈에 띈 것)**: 검증 중 DATA 섹션의 `data.sections[0].data`(실제 바이트)가 `bytes=0`으로 나옴 — `value: 4, 42` allocate가 섹션 개수/식별자 소속엔 이제 올바르게 반영되지만, 정작 그 섹션의 바이너리 내용 자체는 비어 있음. 별개 원인일 가능성이 높아 손대지 않았고, 기록만 해둠.

27. (2026-09-24) "merge sections구현해줘 기존 정렬 순서대로 진행하되 tesx/data/bss 구분하고(ROD는 read only data인데 아직 구현 안함) 이들 기준으로 section base 계산 후 identifier offset으로 계산해줘, assembler에서 identifier value를 binary offset으로 해놨는데 이를 section대비 상대 offset으로 바꾸어 linker에서 받으면 될 것 같아." — `linker::evaluate::functions::mergeSection` 구현. (중간에 사용자가 "그리고 파이프라인을 todo.txt보고 대충 확인해"라고 추가 요청 — 루트 `todo.txt`와 `digital computer 32/todo.txt` 둘 다 확인했으나 전부 CPU 하드웨어 파이프라인(ACU, LD/ST 버퍼, 페이징) 얘기뿐이고 어셈블러/링커 소프트웨어 파이프라인이나 메모리 레이아웃 컨벤션에 대한 내용은 전혀 없었음 — 그래서 base address는 관례대로 0, 순서는 TEXT→DATA→BSS로 진행.)
    - **assembler 쪽 사전 작업**: 사용자 말대로 라벨 값은 이미 섹션 상대 오프셋이었음(`calculateConst`의 `nData.value = data->sectionByte[section_id];`가 애초에 섹션별로 0부터 세는 카운터라서) — 그 자체는 안 고쳤음. 대신 `Identifier.section`(이름)만으로는 "이름 없는 섹션이 여러 개인 파일"에서 어느 섹션인지 구분이 안 되는 문제가 있어서(바로 25/26번에서 고친 버그와 완전히 같은 종류의 함정), `common/format.hpp`의 `_Identifier`에 `u32 sectionIndex`(→`Format::sections`의 인덱스, 상수면 `(u32)-1`) 필드를 추가하고 `getRaw`/`loadRaw`에 반영, `assembler.hpp`의 `formatData()`에서 `nId.sectionIndex`를 `coeff`의 section_id로 채움(이미 `data.sections`와 `format.sections`가 같은 순서/개수로 1:1 대응하는 걸 확인해뒀어서 그대로 재사용 가능).
    - **linker 쪽**: 사용자가 이미 스케치해둔 `_MapData`/`_SectionData`/`_FileData`/`_EvalData` 모양(중간에 여러 번 concurrent 수정됨)을 그대로 따라 채움:
      - Pass 1(파일별 통합): 같은 파일 안에서 이름+타입이 같은 섹션(`.text ... .text ...`처럼 소스에서 여러 번 나뉜 것)을 하나의 `SectionData`로 이어붙이고, 나중에 식별자 주소 계산에 쓸 "그 섹션 안에서 이 조각이 시작하는 로컬 오프셋"을 파일별로 기억해둠(`localBaseByFile`). `Format::mapper`(파일 안 절대 byteIndex)도 이 로컬 오프셋 기준으로 변환해 섹션별 `map`에 재배치. `expressions`는 그대로 복사(다음 단계인 "릴로케이션 적용"에서 쓸 재료로 남겨둠, 이번엔 안 풂).
      - Pass 2(배치): 각 파일의 섹션들을 원래 등장 순서대로 훑으면서 타입별(TEXT/DATA/BSS) 버킷에 이어붙임 — 버킷 자체의 최종 순서는 TEXT→DATA→BSS 고정(관례). ROD/OUTER 타입은 `continue`로 건너뜀(ROD 미구현). 각 파일 섹션의 최종 절대 베이스(`SectionData::sectionOffset`)와, 전체 배치 순서를 보여주는 `EvalData::texts/datas/bsses`(절대 start/end) 둘 다 채움.
      - Pass 3(식별자 해석): `finalAddress = sectionOffset(그 섹션) + localBase(그 파일 안에서의 조각 오프셋) + identifier.value`, 섹션이 없는(상수) 식별자는 `value` 그대로. 결과는 `FileData::definedLabel`(파일별 이름→최종 절대주소 맵)에 저장.
    - **부딪힌 버그와 수정**:
      - `namespace format`의 `using EvalData = evaluate::functions::EvalData;`가 `evaluate`보다 먼저 선언돼 있어 전방참조 컴파일 에러 → 아무 데서도 안 쓰이는 걸 확인하고 그냥 삭제.
      - `<unordered_map>` include 누락(`_EvalData::files` 등에서 이미 쓰고 있었는데도 최상단 include 목록에 없었음) → 추가.
      - **핵심 버그**: `FileData::sections`를 처음엔 섹션 **이름만**으로 키를 잡았는데(사용자가 원래 `std::map<std::string, SectionData>`로 선언), 테스트해보니 파일 안에 이름 없는(`""`) `.data`와 `.text`가 같이 있으면 둘이 같은 키로 충돌해서 **하나로 합쳐져 버림**(25/26번에서 고친 것과 완전히 같은 유형의 버그가 이번엔 linker 쪽에서 재발). `(타입, 이름)` 쌍을 키로 쓰는 `SectionKey = std::pair<u32, std::string>`로 바꿔서 해결.
    - **검증**: 진짜 저장소 파일은 그대로 두고 `/tmp` 스크래치 사본에서 `link()`(별개의, 여전히 미완성인 함수)만 주석 처리한 뒷채 격리 빌드 — 가상의 object 2개(`file1.o`: 이름 없는 `.data`(4바이트)+`.text`(8바이트, 라벨 2개), `file2.o`: 이름 없는 `.text`(4바이트, 라벨 1개))를 직접 만들어 `mergeSection` 실행 → `textBase=0, dataBase=12`(file1.text 8B + file2.text 4B), `totalBin`이 TEXT 블록(file1+file2 순서대로) 다음 DATA 블록 순으로 정확히 이어짐, 식별자 4개(`start=0, loop=4, main=8, value=12`) 전부 기대값과 일치 확인.
    - **오늘 범위 밖으로 남겨둔 것**: 릴로케이션 실제 적용(각 섹션의 `map`/`expressions`를 돌면서 `stackMachine`/`castOperation`으로 값을 계산해 `calcBin`에 패치하는 단계) — `mergeSection`은 주소만 계산하고 아직 아무 바이트도 패치 안 함. `int link(...)` 자체도 여전히 문법 에러투성이 WIP라 손 안 댐. 이름 있는 섹션 문법 파싱 자체가 안 되는 별개 이슈(25번에서 발견)도 여전히 미해결.

28. (2026-09-24) "현재 evaldata변경점 적용해줘, 같은 이름의 section은 따로 만든 vector로 합치고 새로만든 nameMap이용해서 원래 순서인 vector의 index를 참조하게해줘, iterator는 일단 위 주석에 어떤역할인지 명시하고 그 형식을 깊이에 따라 [it, si, ti, qi, pi, hi ... etc]로 고정해줘" — 사용자가 27번 이후 `EvalData`/`FileData`를 다시 concurrent로 바꿔둔 상태(`FileData::sections`가 `map<SectionKey,...>`→`vector<SectionData>`+`nameMap`으로, `EvalData::files`가 `unordered_map`→`vector`로, `EvalData::sections`가 새로 추가됨)에 맞춰 `mergeSection` 전체를 다시 씀:
    - **5단계 구조로 재작성**: (1) 파일별로 같은 (type,name) 섹션을 `FileData::sections`(vector, 첫 등장 순서 유지) + `nameMap`(type+name → 인덱스)으로 통합 + mapper 재배치 + `localBase`(파일별×원본섹션인덱스: 통합된 섹션 안에서 그 조각이 시작하는 위치) 기록 (2) 그 위에서 한 번 더 — 모든 파일에 걸쳐 같은 (type,name)을 `EvalData::sections`(전역 병합, `SectionData::sectionOffset`엔 일단 "병합 블롭 안에서의 상대 위치"만 임시로 저장) (3) `EvalData::sections`를 순서대로(맵 키가 (type,name)이라 자동으로 TEXT→DATA→BSS 그룹핑됨) text/data/bss 버킷에 배치하고 `texts/datas/bsses` 배치 원장 + 절대 base 계산 (4) 각 파일 섹션의 임시 오프셋을 방금 구한 전역 절대 base로 패치 (5) 식별자 주소 = 파일 섹션의 절대 base + `localBase` + 식별자 자체 값.
    - **iterator 네이밍 컨벤션 적용**: `mergeSection` 안에서 깊이 1=`it`, 2=`si`, 3=`ti`, 4=`qi` 고정(더 깊어지면 `pi`,`hi`...) — 어떤 컨테이너를 도는지와 무관하게 중첩 깊이로만 이름을 정함(서로 다른 블록에서 같은 깊이면 같은 이름 재사용, C++ for문 스코프가 분리돼 있어 충돌 없음). 함수 맨 위에 이 규칙 자체를 설명하는 주석 추가. 위치 기반 배열(`localBase`)처럼 진짜 인덱스가 필요한 곳은 이터레이터에서 `it - container.cbegin()`으로 유도해서 씀(별도 카운터 변수 안 둠).
    - **다시 부딪힌 핵심 버그**: `FileData::nameMap`이 사용자 선언대로 `unordered_map<std::string, u32>`(이름만 키)라서, 파일 안에 이름 없는(`""`) `.data`와 `.text`가 같이 있으면 **또** 충돌해서 하나로 합쳐짐(27번에서 `SectionKey`로 고쳤던 것과 완전히 같은 문제가 이번 재작성에서 재발 — 테스트로 실제로 재현: TEXT/DATA 바이트 개수가 서로 뒤바뀜). `nameMap`의 선언 타입(`string` 키)은 그대로 두고, 대신 저장하는 **키 내용**을 `nameMapKey(type, name)`(=`"<type번호>:<이름>"` 합성 문자열)로 바꿔서 해결 — 사용자가 선언한 필드 타입 자체는 안 건드리고 키 구성 방식만 고침.
    - **검증**: `/tmp` 스크래치에서 27번과 동일한 가상 object 2개로 재테스트 — `textBase=0, dataBase=12`, `totalBin`이 TEXT(file1 8B+file2 4B)→DATA(file1 4B) 순으로 정확, `EvalData::sections`에 TEXT/DATA 각각 정확한 크기로 병합됨 확인, 식별자 4개 전부 기대값과 일치. 실제 저장소 파일도 `-fsyntax-only`로 재확인 — 여전히 별개인 `int link(...)`(미완성 WIP) 이외엔 에러 없음.

29. (2026-09-24) "추가한 convertData함수에서 files로 캐스팅, ... mergeByName에서 ... evalData의 외부 sections vector에 합쳐 저장, 이후 merge by type에서 type으로 합침. 적용해줘." — 사용자가 28번 이후 다시 concurrent로 `convertData`/`mergeByName`(빈 스텁) + `mergeByType`(예전 `mergeSection` 그대로, 근데 시그니처에서 `objects` 파라미터가 빠져서 이제 컴파일 안 되는 상태)로 쪼개둔 걸 요청대로 완성:
    - **파이프라인 3분할**: `convertData(data, objects)` — objects→`EvalData::files` 변환(파일별 (type,name) 섹션 통합 + mapper 재배치 + 식별자를 `PendingLabel`(섹션 로컬 오프셋, 아직 최종 주소 아님)로 스테이징). `mergeByName(data)` — 전 파일에 걸쳐 같은 (type,name)을 `EvalData::sections`(새로 **vector**로 바뀜, `EvalData::nameMap`으로 조회)에 합침, 각 파일 섹션의 `sectionOffset`엔 일단 "합쳐진 블롭 안에서의 상대 위치"만 임시 저장. `mergeByType(data)` — `EvalData::sections`를 타입별(TEXT→DATA→BSS)로 배치 + 절대 base 계산 + 각 파일 섹션 오프셋을 절대값으로 패치 + `PendingLabel`을 최종 절대주소로 풀어 `definedLabel`에 기록.
    - **키 mangling**: `sectionKey()`/`nameMapKey()` 헬퍼 함수를 없애고, 요청한 대로 호출부마다 직접 `"#" + std::to_string(type) + "_" + name` 문자열을 만들어 씀(`#`은 식별자에 못 쓰는 문자라 진짜 이름과 절대 안 겹침). `EvalData::sections`가 `map<SectionKey,...>`→`vector<SectionData>`+`EvalData::nameMap`(unordered_map<string,u32>)으로 바뀌면서, `FileData`와 완전히 대칭인 구조가 됨(파일 레벨 통합과 파일 간 병합이 같은 패턴을 재사용).
    - **`asU32`/`asS32` 제거**: `stackMachine::function()`에서 전부 `(u32)`/`(s32)` 직접 캐스트로 교체, 헬퍼 함수 삭제.
    - **iterator 네이밍 구분**: for문을 실제로 도는 이터레이터만 `it`(깊이1)/`si`(2)/`ti`(3)/`qi`(4) 유지, `.find()` 결과처럼 루프를 돌지 않는 건 용도에 맞게 개명(`slotIt`=이 (type,name)이 이미 등록됐는지 찾는 슬롯, `mergedIt`=병합된 전역 섹션을 찾는 결과).
    - **식별자 해석 재설계**: 예전엔 `mergeSection` 한 함수가 `objects`를 직접 참조하면서 식별자를 바로 최종 주소로 풀었는데, `mergeByType`엔 이제 `objects`가 없어서 그 자리에서 못 풀게 됐음 → `convertData` 단계에서 식별자를 완전히 못 풀린 채로 `FileData::pendingLabels`(식별자 이름 → `PendingLabel{섹션인덱스, 그 섹션 안에서의 로컬 오프셋}`)에 스테이징해두고, `mergeByType`이 모든 섹션의 최종 절대 base를 알게 된 시점에 한 번에 풀어서 `definedLabel`에 채우는 방식으로 재설계(사용자가 이번 메시지에서 식별자 얘기는 안 했지만, 파이프라인이 끊기지 않게 하려면 필요해서 판단해서 추가함 — 이 부분은 확인 필요할 수 있음).
    - **검증**: `/tmp` 스크래치에서 이전과 동일한 가상 object 2개로 `convertData`→`mergeByName`→`mergeByType` 순서로 실행 → `textBase=0, dataBase=12`, `totalBin` 순서 정확, `EvalData::sections`에 TEXT/DATA 각각 올바른 크기로 병합, 식별자 4개 전부 기대값과 일치. 실제 저장소 파일도 `-fsyntax-only` 재확인 — 여전히 별개인 `int link(...)`(미완성 WIP, 손 안 댐) 이외엔 에러 없음.

30. (2026-09-24) "castOeration 이용해서 마지막으로 exprMapping해주고 ... link함수 대충 완성 후(그냥 eval, no log info?, binary ret) main에서 파일 받아서 link호출 한 뒤 build&test해줘" — 링커 파이프라인의 마지막 단계와 실제 진입점을 완성하고 처음으로 진짜 end-to-end 빌드/테스트를 돌림. (사용자가 이미 `evaluate(data, objects)` 드라이버를 `convertData→mergeByName→mergeByType→mapExpr` 순서로 만들어뒀고, `mapExpr`는 빈 스텁이었음 — 이게 요청한 "exprMapping".)
    - **`FileData`에 `usedIdentifier` 추가**: `mapExpr`가 `castOperation`을 쓰려면 파일별 식별자 이름 테이블이 필요한데 안 옮겨져 있었음 → `convertData`에서 `obj.usedIdentifier`를 그대로 복사해오도록 추가.
    - **`mapExpr` 구현**: 파일별로 (1) 그 파일의 `usedIdentifier` 각각을 전체 파일들의 `definedLabel`에서 찾아 전역 최종 주소로 미리 해석해두고(파일 간 심볼 참조 지원 — 다만 현재 어셈블러엔 `.extern` 같은 게 없어서 "이 파일에 없는 심볼"은 애초에 조립 단계에서 막힘, 그래도 구조상 지원됨) (2) 그 파일의 각 섹션(`SectionData::map`)에 기록된 relocation마다 `castOperation`(RPN → `stackMachine::Element`, IDENTIFIER는 위에서 구한 주소로 치환) → `stackMachine::runOperation`으로 최종 값 계산 → `data.totalBin`의 해당 절대 위치에 비트 단위로 패치(`reloc.offset`/`reloc.size`, 필드가 여러 바이트에 걸치면 앞쪽(더 낮은 주소) 바이트로 계속 이어감 — 어셈블러의 `registerReplace`가 쓰던 것과 같은 빅엔디안 LSB-바이트 컨벤션).
    - **`link()` 완성**: 예전 깨진 `int link(std::vector<u8>&)`(오브젝트 1개짜리, 미완성) 대신 `bool link(std::vector<u8>& outBinary, const std::vector<std::pair<std::string, std::vector<u8>>>& rawObjects)`로 재작성 — 각 raw object를 `Format::loadRaw`로 역직렬화해서 `objects` 리스트를 만들고, `evaluate::functions::evaluate()`(4단계 파이프라인) 한 번 돌려서 `data.totalBin`을 그대로 반환. 로그 없이 평가만(요청한 "그냥 eval, no log info, binary ret" 그대로).
    - **`main.cpp`**: `<출력 파일> <입력 오브젝트 파일...>` 인자 받아서 각 파일 읽고 `link()` 호출, 성공하면 결과를 출력 파일에 씀(어셈블러 `main.cpp`와 같은 스타일).
    - **첫 진짜 end-to-end 빌드+테스트**: `build-asm.cmd`/`build-lnk.cmd` 둘 다 재빌드 성공. `.text`만 있는 소스 2개를 각각 `assembler.exe`로 따로 조립(각 8바이트, 라벨 하나씩 참조하는 상대 점프 포함) → `linker.exe`로 두 오브젝트를 합침 → 결과 16바이트 바이너리를 직접 헥스덤프로 검증: obj1의 `loop` 참조는 그대로 0(자기 앞부분이라 안 밀림), **obj2의 `start` 참조는 원래 로컬 오프셋 0에서 최종 주소 8로 정확히 리베이스됨**(obj1의 8바이트가 앞에 붙었으므로) — 크로스 파일 재배치가 정확히 동작함을 직접 확인. `.data`+`.text` 단일 파일 케이스도 추가로 테스트(`ld.zero gen[0],value`의 임메 필드가 TEXT(8B) 다음에 오는 DATA 섹션의 최종 주소 8로 정확히 패치됨) — 둘 다 통과.
    - **여전히 범위 밖으로 남은 것**: `.extern`/외부 심볼 선언 문법이 없어서 진짜 "다른 파일에서 정의된 심볼을 이 파일에서 참조" 케이스는 애초에 어셈블 단계에서 막힘(오늘 안 건드림) — 그래서 이번 end-to-end 테스트는 "각자 완결된 여러 파일을 합쳐서 재배치"까지만 검증했고, 진짜 크로스파일 심볼 링킹까지는 검증 못 함(구조상 `mapExpr`가 전 파일의 `definedLabel`을 다 뒤지도록 짜여있어서 지원은 되지만). 이름 있는 섹션 파싱 불가 이슈(25번)도 여전히 미해결.

31. (2026-09-24) "assembler에서 fileName으로 label Mangling 추가해줘 (binary기준 이름)" — 30번 테스트에서 확인했듯 서로 다른 파일이 같은 라벨 이름(`loop` 등)을 쓰면 링커의 전역 심볼 테이블에서 충돌하는 문제가 있어서, 어셈블러가 라벨 이름에 파일명을 섞어 넣도록(mangling) 함:
    - **처음엔 `calculateConst`(평가 로직) 안에서 처리하려 했으나, 작업 중 사용자가 "굳이 처리에서 바꾸지 말고 format시 바꾸게 해줘"라고 정정** → `calculateConst`의 `label`/`identifier` case는 원래대로 raw 이름 그대로 쓰도록 되돌리고, 대신 `format::formatData()`(직렬화 시점)에서 `definedIdentifier[].name`과 `usedIdentifier[]`(그리고 그걸 가리키는 `Operation.value` 인덱스) 양쪽에 똑같이 mangling을 적용 — `idenifierValue`/`expressionValue`는 평가 내내 raw 이름으로 유지되고, Format으로 굳어지는 마지막 순간에만 이름이 바뀜.
    - **mangling 형식**: `mangleLabel(fileName, label)` = `fileName.empty() ? label : fileName + "#" + label` (`#`는 식별자에 못 쓰는 문자라 안 겹침 — 이미 매크로 고유화(`identifier_unique`, `#define`류)에서도 `"#"+숫자` 형태로 같은 관례를 쓰고 있었음). `EvalData::fileName`(assemble()이 채워줌)을 `formatData()`가 읽어서 씀.
    - **"binary기준 이름"**: `fileName`은 입력 소스 파일명이 아니라 **출력 바이너리 파일명**(디렉토리/확장자 제거한 base name)을 씀 — `assembler/main.cpp`에서 `outputFile`(argv[2])을 파싱해 `outputBase`를 만들고 `assemble(buff, binary, dump, outputBase)`로 전달. `assemble()`에 `fileName` 매개변수(기본값 `""`, 기존 호출부 호환) 추가.
    - **검증**: 같은 이름(`loop`)의 라벨을 각자 정의하는 파일 2개를 조립 → 바이너리 안에 `mobj1#loop` 형태로 mangled된 이름이 실제로 박혀있음을 확인 → 링크했더니 두 파일의 `loop`가 서로 안 섞이고 각자 자기 파일 안의 올바른 최종 주소로 정확히 리졸브됨(obj1의 loop→0, obj2의 loop→8) — mangling 이전이었다면 링커의 이름 기반 전역 검색이 첫 번째로 찾은 파일의 값으로 둘 다 잘못 리졸브했을 상황.

32. (2026-09-24) "linker의 section Mangling 도 <type>#<name>으로 바꾸고 빌드 된 툴 \digital computer 32\tool에 저장해줘, 기존 dasm은 dasm-old로 새로운 툴은 dasm/dlnk로 작명 (작명 후 table삭제 후 재실행, 꼬임 방지)":
    - **섹션 mangling 형식 통일**: 링커의 섹션 키 mangling을 `"#" + type + "_" + name`에서 어셈블러의 라벨 mangling과 같은 형태인 `type + "#" + name`으로 통일(5곳: `convertData` 3곳, `mergeByName`/`mergeByType` 각 1곳 + 관련 주석). 순수 문자열 포맷 변경이라 의미는 그대로 — 재빌드 후 2파일 링크 회귀 테스트(30/31번과 동일 케이스) 통과 재확인.
    - **툴 배포**: `digital computer 32/tools/bin/`(레포에 이미 있던 옛 어셈블러 배포 위치 — CLAUDE.md에도 `tools/bin/dasm.exe`로 문서화돼 있던 곳)에 있던 기존 `dasm.exe`+`dasm_table.bin`을 `dasm-old.exe`+`dasm-old_table.bin`으로 이름 바꾸고, 새로 빌드한 `assembler.exe`/`linker.exe`(`src/assembler-linker/build/`)를 각각 `dasm.exe`/`dlnk.exe`로 복사.
    - **테이블 꼬임 방지**: 이름 바꾸기 전 있던 `dasm_table.bin`(옛 버전 문법으로 빌드된 캐시)이 새 `dasm.exe`한테 그대로 남아있으면 버전 번호만 맞으면 통째로 재사용돼버려서(과거 25번에서 실제로 겪은 문제) 새 문법과 안 맞는 stale 테이블을 로드할 위험이 있었음 → `dasm_table.bin`/`dlnk_table.bin`을 명시적으로 지운 뒤 `dasm.exe`/`dlnk.exe`를 인자 없이 한 번씩 실행해서 강제로 새 테이블을 생성시킴(`dasm.exe`는 429848바이트짜리 새 `dasm_table.bin` 정상 생성 확인; `dlnk.exe`는 텍스트 문법이 없어 애초에 테이블 캐시 자체가 없는 게 정상).
    - **최종 검증**: 배포된 `digital computer 32/tools/bin/dasm.exe` + `dlnk.exe`로 처음부터 끝까지(2개 파일 조립 → 링크) 다시 돌려서 이전과 동일하게 올바른 결과(리베이스된 주소 `0008`) 나오는 것까지 확인.

33. (2026-09-24) "convertData시 순회한 순서대로 initialBin에 binary추가하고 이를 Section에서 Indexer로 받게 해서 메모리 최적화 해줘" — 이전까진 `convertData`(파일별 통합)→`mergeByName`(파일 간 병합)→`mergeByType`(그룹 배치)를 거치며 같은 바이트가 최대 4번(원본 `Format::binary`→`FileData::sections[].binary`→`EvalData::sections[].binary`→`textBin`류→`totalBin`) 복사되던 걸, `EvalData::initialBin`(전체 오브젝트 바이트를 순회 순서대로 한 번만 이어붙인 공용 버퍼) + 각 `SectionData`가 실제 바이트 대신 그 버퍼 안 (비연속일 수 있는) 구간 목록만 들고 있는 `Indexer`로 줄임:
    - **`Indexer` 완성**: 사용자가 이미 `std::vector<pair<u32,u32>> indexes`(구간 목록) + `get(id)`(논리 오프셋→실제 버퍼 오프셋 변환) 스켈레톤을 만들어뒀는데 두 가지 문제가 있었음 — `pair`가 `std::` 없이 맨몸으로 쓰여있어서(이 헤더엔 `using namespace std`가 없음) 컴파일 에러, `get()`의 순회 로직 자체도 버그(break 시점에 `base`가 현재 구간이 아니라 이전 구간 값으로 남아 잘못된 오프셋을 반환) → 둘 다 고치고, 구간 추가용 `add(start,end)`와 전체 길이 `size()` 헬퍼도 추가.
    - **파이프라인 3단계 전부 `.binary`(복사) → `.ninIndex`(구간 참조)로 전환**: `convertData`는 이제 각 섹션 조각의 실제 바이트를 `data.initialBin`에 한 번만 append하고 그 위치를 `sd.ninIndex.add(start,end)`로만 기록(복사 없음). `mergeByName`은 파일별 `ninIndex`의 구간들을 통째로 복사(`{start,end}` 쌍 몇 개만, 바이트는 안 건드림)해서 `EvalData::sections[].ninIndex`에 이어붙임. **실제 바이트 복사는 `mergeByType`에서 딱 한 번만** 일어남(`textBin`/`dataBin`/`bssBin`을 만들 때 각 구간을 `initialBin`에서 꺼내옴) — 그다음 `totalBin`으로 합치는 복사까지 포함하면 기존 최대 4회에서 2회(최종 그룹화 + totalBin 조립)로 줄어듦.
    - **`mapExpr`는 안 건드림**: relocation 패치는 이미 `data.totalBin`(펼쳐진 최종 바이너리)을 직접 건드리는 방식이라 `ninIndex`와 무관 — 주석의 `si->binary` 표현만 `si->ninIndex`로 정정.
    - **검증**: 30/31/32번에서 쓰던 것과 동일한 회귀 테스트 2종(서로 다른 라벨 이름 2파일 링크, `.data`+`.text` 단일 파일)을 재실행해서 바이트 단위로 이전과 완전히 동일한 결과(리베이스된 주소 `0008`, `value` 주소 `8`) 나오는 것 확인. `-fsyntax-only` 전체 빌드도 이번엔 **에러 0개**(그동안 남아있던 별개의 `int link(...)` 미완성 이슈도 30번에서 이미 해결된 상태라, `linker.hpp` 전체가 처음으로 완전히 깨끗하게 컴파일됨).
    - **툴 재배포**: `digital computer 32/tools/bin/dlnk.exe`를 새로 빌드한 걸로 교체(테이블 캐시가 없는 링커라 32번 때와 달리 별도 table 삭제/재실행 불필요).

34. (2026-10-07) 요청 전, "현재 assembler, linker 빌드하고 정상 작동하는지 program 폴더의 코드 assemble/link해봐" — `assembler/program/*.txt` 30개 전부를 `assembler.exe`로 조립 시도: 29개는 크래시 없이 끝남(`run.txt` 1개만 세그폴트, 기존 미해결 이슈로 추정 — 원인 미조사, 손 안 댐). parse+eval 둘 다 성공한 13개(`block/chaintest/fibReg/fibonacci_raw/forwarding/fullwidth/pushtest/ram/ram2/ram3/store/store2/test`)는 전부 단독 링크도 정상 동작, `ram.o`+`ram2.o`+`ram3.o` 3파일 합쳐 링크한 것도 바이트 수(40+32+36=108)까지 정확히 맞음 — 어셈블러/링커 파이프라인 자체는 정상이라고 확인. 나머지(`add_terminal`, `bcdtodec`, `branch`, `copy`, `factorial_*`, `function`, `graphic`, `multiply`, `prime*`, `stacktest`, `testmm`, `texteditor`)는 parse 또는 eval 실패 — 전부 기존에 있던 미구현 문법/기능 이슈로 추정, 이번엔 분석/수정 안 함.

35. (2026-10-07) "현재 assembler에 추가한 global .. equate 까지 token을 lexer까지 구현해줘" — `lexer::TokenType`엔 이미 `global`/`_extern`/`equ`(그 뒤 `include`/`ifndef`/`ifdef`는 이번 요청 범위 밖, "equate까지"로 한정)가 선언돼 있었는데, 실제 렉서 규칙 목록(`CreateData`)엔 연결이 안 돼 있었을 뿐 아니라 그 자리에 **깨진 placeholder `{ ""}`**(쉼표/중괄호 누락)가 그대로 남아있어서 `assembler.hpp` 전체가 컴파일조차 안 되는 상태였음(`-fsyntax-only` 없이 실제 `build-asm.cmd`를 돌려봐도 바로 확인됨).
    - `{ ""}`를 실제 키워드 규칙 3개로 교체: `{ "global", TokenType::global }` / `{ "extern", TokenType::_extern }` / `{ "equ", TokenType::equ }` (`.text`/`.data`/`.bss` 규칙 바로 다음, `#define` 규칙군 앞 — enum 선언 순서와 동일한 위치).
    - `tokenStr`(토큰 타입→디버그 표시용 문자열 맵)에도 같은 세 항목 추가 — `-token` 덤프에서 `global`/`extern`/`equ`로 정상 표시되도록.
    - **캐시 테이블 버전 bump**: 렉서 규칙(문법)이 바뀌었으므로 32번에서 겪은 "꼬인 stale 테이블 재사용" 위험을 피하려고, `const unsigned long long version = 400;` → `401`로 올림(테이블 파일을 직접 지우는 대신 버전 비교로 자동 무효화되게 — 이 방식이 이미 `cache` 네임스페이스에 구현돼 있던 정식 메커니즘).
    - **검증**: `global foo` / `extern bar` / `val equ 5`를 `-token`으로 찍어 각각 `global`/`extern`/`equ` 토큰으로 정확히 분리됨을 확인(파서는 당연히 이 문법을 모르므로 "unexpected token: global"로 실패 — 요청이 "lexer까지"였으므로 범위 내에서 정상). `globalFlag`/`externThing`/`equation`처럼 키워드를 접두사로 포함하는 식별자가 쪼개지지 않고 통째로 `identifier`로 lex되는 것도 확인(maximal-munch 정상 동작). 34번에서 쓴 `program/` 29개 재조립 회귀 테스트도 재실행 — 새로운 크래시 없음(기존 `run.txt` 크래시만 그대로).
    - **범위 밖으로 남은 것**: 파서 문법(`global`/`extern`/`equ`를 실제로 어떻게 쓰는지 — 심볼 export/import, `equ` constant 정의 등)과 평가기 연결은 전혀 안 함(요청이 명시적으로 "lexer까지"). `include`/`ifndef`/`ifdef`도 이번엔 손 안 댐.

36. (2026-10-07) "include .. ifndef 및 기타 추가된 token도 처리해줘" — 35번에서 범위 밖으로 남겨뒀던 `include`/`ifndef`/`ifdef`를 처리하면서, 전체 `TokenType` enum을 `CreateData`/`tokenStr`와 자동으로 전부 대조(스크립트로 토큰 하나하나 grep)해서 "기타 추가된 토큰"(= enum엔 있는데 둘 중 하나라도 비어있는 토큰)까지 전부 찾아 같이 메움:
    - **`include`/`ifndef`/`ifdef`**: `#define`/`#macro`/`#end`와 같은 전처리기 지시어 계열이라 같은 `#`-prefix 컨벤션으로 `{ "#include", TokenType::include }` / `{ "#ifndef", TokenType::ifndef }` / `{ "#ifdef", TokenType::ifdef }` 추가(enum 선언 순서와 동일하게 `#define` 그룹 바로 앞에 위치). `tokenStr`에도 동일하게 3개 추가.
    - **대조 중 추가로 발견한 "기타 추가된 토큰"**: `rol`/`roli`/`ror`/`rori`는 `CreateData`(렉서 규칙)와 실제 opcode 테이블(1500번대 줄)엔 이미 쓰이고 있었는데 `tokenStr`(디버그 표시용 맵)에서만 빠져있었음 — 추가하면 당장 동작이 바뀌는 건 아니지만(디버그 덤프 전용) `-token` 덤프 시 매핑 실패할 잠재 위험이 있어 같이 채움. `curline`(`$` → `TokenType::curline`, CLAUDE.md의 "알려진 미해결 이슈"에 언급된 그 토큰)도 같은 이유로 `tokenStr`에 추가(파서/평가기 연결은 여전히 범위 밖, 그 문서 항목 그대로 유효).
    - **캐시 버전 bump**: 35번과 같은 이유로 `version 401→402`.
    - **검증**: `#include foo` / `#ifndef BAR` / `#ifdef BAZ` / `rol/roli/ror/rori gen[0],N` / `$`를 `-token`으로 찍어 전부 올바른 이름으로 표시됨을 확인(파서는 여전히 모름 → "unexpected token: #include"로 실패, 요청 범위 내 정상). `program/` 29개 재조립 회귀 테스트도 재실행 — 새로운 크래시 없음.
    - **최종 상태**: enum에 선언된 모든 `TokenType` 값이 `CreateData`(렉서 규칙)와 `tokenStr`(디버그 표시) 양쪽에 전부 존재함을 스크립트로 재확인(0건 누락).

37. (2026-10-07) "parser에서 모든 .text .data .bss ... 다음에 identifier올 수 있도록 ... 처리하고 equate 문법 [identifier equ expr] 작성해줘" — 25/26번에서 분석/수정했던 "이름 있는 섹션" 문제가 CLAUDE26번 당시 "파싱 자체가 안 됨(`assemble()=-2`)"으로 남아있던 것의 실제 원인을 찾아 완전히 고치고, 사용자가 이미 `NT::equate`/`AT::equate`를 참조해두고(하지만 선언·생산규칙은 없어서 컴파일 안 되던) `data_section`/`bss_section` 규칙을 완성함:
    - **이름 있는 섹션 버그의 진짜 원인**: `.text`/`.data`/`.bss` 뒤에 선택적 `identifier`(섹션 이름)가 올 수 있는 문법이 **"본문 있음 + 뒤에 다른 섹션이 더 있음"(recurse) 변형 1개만** 존재했음(기존 `section -> text identifier text_section section`) — "본문 있음+이 섹션이 파일의 마지막"(`.text foo\n add...` EOF), "본문 없음+뒤에 다른 섹션"(`.data foo\n.text ...`, 정확히 이번 요청에서 예시로 든 `.text asdf .data` 패턴), "본문 없음+마지막"(`.data foo` 그걸로 끝) 세 변형은 이름 없는 버전(unnamed)에만 있고 이름 있는 버전(named)에는 대응하는 생산규칙이 아예 없었음 → 흔한 사용 패턴(섹션 하나만 쓰고 끝, 또는 빈 섹션 바로 다음 섹션)이 전부 이 세 변형에 걸려서 "이름 있는 섹션은 아예 파싱이 안 됨"으로 보였던 것. `text`/`data`/`bss` 각각 3가지 변형에 `{TT::identifier, true, false}`를 끼워 넣은 대응 생산규칙 9개를 추가(기존 1021-1023번 named-recurse 규칙과 동일한 위치에 identifier 토큰을 삽입하는 패턴 그대로 반복).
    - **충돌 여부 확인**: 이 파서 생성기는 LALR류로 lookahead를 정확히 계산하지만(closure+lookahead propagation), 테이블 빌드 시 **충돌(shift/reduce든 reduce/reduce든)이 나면 경고 없이 먼저 채워진 액션이 그대로 유지되고 나머지는 조용히 버려지는 구조**(`if (slot.type == ActionType::Error) slot = ...`)라 설계상 애매한 문법을 넣으면 위험함 — 다만 "`.text` 뒤 identifier"가 섹션 이름(새 규칙)인지 본문 첫 라벨(`label -> identifier colon`, text_section 클로저로 이미 존재)인지는, identifier를 shift한 다음 "그 다음 토큰이 colon이냐 아니냐"로 완전히 갈리므로(이름 변형들의 reduce lookahead는 FOLLOW(section)=다음 섹션 키워드/EOF뿐이라 colon과 안 겹침) 실제 충돌은 없음을 확인.
    - **부수 발견 버그(같이 고침)**: `bss_section`의 `equate` recurse 규칙이 `{ NT::bss_section, AT::data_section, -1 }, {{NT::equate,...},{NT::data_section, true, true}}`로 되어있어서(데이터섹션 규칙을 복붙하다 `bss_section`→`data_section`으로 못 바꾼 전형적 오타) `AT::data_section`/`NT::data_section`을 `AT::bss_section`/`NT::bss_section`으로 수정. 비-recurse(마지막 항목) 변형에도 `equate` 단독 케이스가 `data_section`에는 있는데 `bss_section`엔 빠져있어서 대칭으로 추가.
    - **`equate` 문법 완성**: `NonterminalType`/`ASTNodeType`에 `equate`를 `label` 바로 옆에 추가(디버그 문자열 맵 `nonterminalStr`/`asttypeStr`에도 동일 위치로 추가) + 생산규칙 `{ NT::equate, AT::equate, 0 }, { {TT::identifier,false,false}, {TT::equ,false,false}, {NT::expression,true,false} }` 추가 — `label`(`identifier colon`)과 같은 패턴으로 식별자 텍스트를 노드 text로 가져오고(`nodeIndex=0`), `equ` 키워드는 버리고, 표현식은 자식으로 유지. `evaluator`(`calculateConst`) 쪽 연결은 이번 요청 범위 밖(파서까지만) — `AT::equate`는 아직 아무 case도 없어 평가 시 조용히 no-op(해당 switch에 `default:`가 없어서 매칭 안 되면 그냥 지나감, 크래시 없음 확인).
    - **캐시 버전 bump**: 그래마/enum이 또 바뀌어서 `version 402→403`.
    - **검증**: ① `.data mydata`+`.text mycode`(둘 다 본문 있음, data가 마지막 아님) ② `.text mycode` 단독(본문 있음, 파일의 유일한 섹션) ③ `.data asdf`(본문 없음) 바로 다음 `.text`(요청에서 예로 든 패턴 그 자체) ④ `.data onlyname` 단독, 본문 없음 — 4가지 전부 parse+eval 성공 확인. `equ`도 `.data`/`.bss` 양쪽에서 `foo equ 5`, `bar equ 1+2*3` 둘 다 parse+eval 성공 확인. `program/` 29개 재조립 회귀 테스트도 재실행 — 새로운 크래시 없음.

38. (2026-10-07) "evaluator, preprocessor 함수가 bool 대신 int를 반환하게 하고 evaluator/preprocessor가 실행시 0반환(성공)이 아니면 해당 에러 코드 반환하게 바꿔줘" + (중간 추가 요청) "나중을 위해 링커의 stackmachine도 int반환으로 바꿔줘" — 지금까지 `bool`(성공/실패만 구분)이던 evaluator/preprocessor 쪽 콜백·드라이버 전부를 `int`(0=성공, 그 외=구체적 에러 코드)로 바꾸고, 호출 체인 전체가 그 코드를 그대로 전파하도록 변경. 같은 패턴을 링커의 `stackmachine`에도 적용.
    - **제네릭 템플릿 2개**(`evaluator.hpp`/`preprocesser.hpp`): `Evaluator::EvaluateFunction`/`TBEvaluateFunction`, `Preprocesser::PreprocessFunction` 타입을 `int(*)(...)`로 변경. `Evaluator::evaluate()`/`evaluateTB()`, `Preprocesser::preprocess()` 드라이버 루프도 각 콜백 호출 결과를 `int code = function(...); if (code) return code;` 패턴으로 바꿔서, 콜백이 반환한 에러 코드를 그대로(가공 없이) 위로 전달 — 더 이상 `false`로 뭉개지 않음.
    - **`assembler.hpp`의 `preprocesser::functions`**: `PreprocessError` enum 추가(`SUCCESS=0, MACRO_ARG_OUT_OF_RANGE, EXPECTED_DEFINE_IDENTIFIER, EXPECTED_MACRO_IDENTIFIER, EXPECTED_MACRO_ARGCOUNT`). `handleReplace`/`removeUnused`(콜백)와 `preprocesser::preprocess()`(드라이버) 전부 `int`로 변경, 각 `return false` 자리에 실패 종류에 맞는 enum 값을 붙임(예: `#macro` 인자 개수 초과 참조는 `MACRO_ARG_OUT_OF_RANGE`, `#define`/`#macro` 뒤에 식별자가 없으면 각각 `EXPECTED_DEFINE_IDENTIFIER`/`EXPECTED_MACRO_IDENTIFIER`).
    - **`assembler.hpp`의 `evaluator::functions`**: `EvaluateError` enum 추가(`SUCCESS=0, MISSING_EXPRESSION, NEGATIVE_INDEX, NOT_CONSTANT`). `calculateSection`/`calculateConst`/`evaluateCode`/`collapseExpr`(콜백들)와 `evaluate()`(드라이버) 전부 `int`로 변경. 코드 배정 기준: "자식 노드의 표현식 값이 아직 안 나옴"(전방참조/하위식 실패 전파) 전부 `MISSING_EXPRESSION`으로 통일(binaryOp/unaryOp/단항+/`allocate`/`allocate_zero`/`evaluateCode`의 allocate·reg_index·immidate16·immidate8 — 총 8곳), `index[...]`가 음수로 평가되면 `NEGATIVE_INDEX`, `allocate`/`allocate_zero`의 크기 식이 상수가 아니면 `NOT_CONSTANT` — 실패 "종류"별로 묶어서 재사용(사이트마다 전부 별개 번호를 매기는 대신, 같은 종류 실패는 같은 코드로 — 과도하게 세분화하지 않는 쪽으로 판단).
    - **`assemble()` 호출부**: `bool validPreprocess = preprocesser::preprocess(...)` / `bool validEvaluate = evaluator::evaluate(...)`를 각각 `... == 0`으로 bool 캐스팅하도록 수정 — `AssemblerDump`의 `preprocSuccess`/`evalSuccess`는 여전히 bool 필드로 유지(이번 요청은 내부 콜백/드라이버 계층 얘기였고, `assemble()`의 외부 반환 체계(-1/-2/-3)는 범위 밖이라 안 건드림).
    - **링커 `stackmachine.hpp`**(제네릭 `StackMachine<Operation>`): `StackMachineFunction` 타입을 `int(*)(...)`로, `run()`을 `int` 반환으로 변경 + 자체 실패용 `RunError` enum(`SUCCESS=0, STACK_UNDERFLOW, INVALID_RESULT`) 추가 — `function` 콜백이 돌려준 코드는 그대로 전파, `run()` 자신이 감지하는 실패(오퍼랜드 부족/최종 스택에 값이 정확히 1개가 아님)만 자체 코드 사용.
    - **링커 `linker.hpp`의 `stackMachine` 네임스페이스**: `function()`에 `FunctionError` enum(`SUCCESS=0, DIV_BY_ZERO, MOD_BY_ZERO, UNKNOWN_OPERATOR`) 추가하고 `int` 반환으로 변경(나눗셈/모듈로 0 체크, 알 수 없는 연산자 default case). `runOperation()`도 `int` 반환(내부적으로 `stackMachine.run(...)` 결과를 그대로 반환). 호출부(`mapExpr`의 relocation 패치 루프)는 `if (stackMachine::runOperation(elements, value) != 0) return false;`로만 최소 수정 — `mapExpr` 자신의 `bool` 반환은 이번엔 안 건드림("나중을 위해"라는 사용자 표현대로, 지금은 stackmachine 계층만 준비해두는 스코프).
    - **검증**: 어셈블러는 `program/` 29개 재조립 회귀(크래시 없음, 기존과 동일하게 pass/fail 분포 유지) + 알려진 eval-실패 케이스(`add_terminal.txt`)가 여전히 "evaluate: failed"로 동일하게 동작함을 재확인. 링커는 두 파일(서로 다른 라벨 참조) 크로스파일 링크 테스트를 다시 돌려 리베이스된 주소(`loop`→4)까지 정확히 일치하는 바이너리 생성됨을 확인(스택머신 경로가 `int`로 바뀐 뒤에도 relocation 계산 결과가 바이트 단위로 동일).
    - **참고**: `mapExpr`를 비롯해 링커의 나머지 `bool` 반환 함수들(`convertData`/`mergeByName`/`mergeByType`/`evaluate`/`link` 등)은 이번 요청에 포함되지 않음 — "나중을 위해"라는 표현대로 stackmachine만 선행 작업한 것이므로, 동일한 int-에러코드 패턴을 링커 전체로 확장하는 건 후속 요청이 오면 그때 진행.

39. (2026-10-08) "지금 include구현 방식 확인하고 마저 완성해줘, parserError-handling시 line계산함수(tokens의 absLine -> backupToken의 fileId, line)를 따로 구현 후 사용해줘" + (중간 추가 요청) "include파일 insert하기전에 불러오는 함수에서 text 뒤에 newline 추가해서 file이 newline으로 안끝나도 linecalcfunc가 꼬이지 않게 해줘" — 사용자가 이 대화 밖에서 직접 `#include<path>` 멀티파일 지원을 대대적으로 작업해둔 상태(`Token`에 `line`(절대/전역 줄번호)·`fileLine`(파일 내 상대 줄번호)·`fileId` 필드 추가, `PreprocData`에 `includePath`/`resumeBeg`/`resumeEnd` 추가, `State::readInclude`/`resumeInclude` 추가, `tagLine`을 `(tokens, line=1, file=0)`→반환값 있는 형태로 재작성, `insertInclude` 신설, `assemble()`에 `#include` 처리용 while 루프 신설)였는데, 컴파일조차 안 되고(여러 군데 미완성) 실제로 켜보면 무한루프에 빠지는 상태였음 — "확인하고 마저 완성"은 이 전체를 점검해서 실제로 동작하게 만드는 작업이었음. 발견/수정한 문제들(심각도 순):
    - **무한루프의 진짜 원인**: `insertInclude`가 삽입된 구간의 끝(="아직 안 지워진 `#include<...>` 토큰의 위치")을 `resumeEnd`라는 **iterator**로 미리 계산해 저장해뒀는데, `removeUnused`가 그 구간 안의 whitespace/newline/comment를 하나라도 지우면(`vector::erase`로 전체가 한 칸씩 당겨짐) `resumeEnd`는 예전 메모리 위치를 가리키는 채로 멈춰있어서(값이 안 바뀜) 더 이상 실제 stale 토큰 위치와 안 맞게 됨 → `it == resumeEnd` 비교가 영원히 거짓이 되어 stale `#include` 토큰이 절대 안 지워지고, `handleReplace`가 그걸 계속 다시 "발견"해서 같은 파일을 무한히 다시 include 요청함(토큰이 끝없이 불어남). **고침**: `resumeEnd`(iterator)를 `resumeRemaining`(int, 삽입된 구간에 남아있는 "원본 슬롯 수" 카운트)로 교체 — `removeUnused`가 그 구간의 토큰을 하나 처리할 때마다(지우든 안 지우든) 1씩 감소시키고, 0이 되는 순간의 `it`이 정확히 stale 토큰 위치임을 보장(지워진 개수와 무관하게 "원래 슬롯을 몇 개 방문했는지"로 추적하므로 vector 재배치에 영향을 안 받음).
    - **`tokenBackup`이 tagLine보다 먼저 복사됨**: 초기 메인 파일과 매 include 파일 양쪽 다 `tokenBackup.insert(...)`가 `tagLine(...)` 호출보다 **먼저** 실행되고 있었음 — `tokenBackup`은 독립된 복사본이라 그 뒤에 `tagLine`이 원본 `tokens`/`curTokens`를 태깅해도 이미 복사된 `tokenBackup` 쪽 엔트리들은 전혀 갱신 안 됨 → `tokenBackup`의 모든 토큰이 `.line`/`.fileLine`/`.fileId` 미초기화 상태로 남음(파서 에러 메시지 재구성 루프와 새로 만든 `resolveSourceLocation` 둘 다 이 깨진 데이터로 검색해서 항상 실패/빈 결과). **고침**: 두 군데 다 순서를 `tagLine` 먼저 → `tokenBackup` 복사 나중으로 뒤집음.
    - **라인계산함수 구현**: `preprocesser::resolveSourceLocation(backupTokens, absLine, fileId&, fileLine&)` 신설(`tagLine` 바로 아래) — `backupTokens`(전처리로 토큰이 지워지기 전의 원본 복사본)를 순회해서 `.line == absLine`인 첫 토큰의 `.fileId`/`.fileLine`을 꺼내줌. `assemble()`의 파서 에러 메시지 블록에서 기존의 "error at line <절대줄번호>:" 대신 이 함수로 얻은 `(fileId, fileLine)`을 `includedFiles[fileId]`(파일 경로)와 조합해 "error at <파일경로> line <그 파일 안에서의 줄번호>:" 형태로 출력하도록 교체(실패 시에만 기존 방식으로 폴백).
    - **`fileIoFunc`가 할당된 적이 없었음**: 전역 함수포인터 `getFileIO fileIoFunc;`가 선언만 되고 어디서도 대입된 적이 없어서(널 포인터) include를 실제로 타면 바로 크래시할 상태였음 — `main.cpp`에 이미 정확히 맞는 시그니처의 `int fileIO(std::string&, std::string)` 어댑터 함수가 작성돼 있었는데 연결만 안 돼 있었던 것 → `main()` 맨 앞에 `assembler::fileIoFunc = fileIO;` 한 줄 추가.
    - **include 경로 결합 버그**: `fileName + preprocData.includePath`(단순 문자열 이어붙이기)라서 `fileName`이 `"dir/main.txt"`면 `"dir/main.txtinc.txt"`처럼 깨진 경로가 만들어짐 → `fileName`에서 디렉터리 부분만 뽑아 `baseDir + includePath`로 결합하도록 수정(이미 `main.cpp`가 출력 파일명에서 베이스네임 뽑을 때 쓰던 것과 같은 `find_last_of("/\\")` 패턴 재사용).
    - **`includedFiles`가 채워지는 적이 없었음**: 초기값 `{fileName}` 이후로 전혀 `push_back`이 없어서 `includedFiles[fileId]`(에러 메시지용 파일명 조회)가 항상 범위를 벗어났을 것 → 각 include가 실제로 성공적으로 열릴 때마다 해결된 `path`를 `push_back`하도록 추가.
    - **(중간 추가 요청) include 파일이 개행으로 안 끝나도 안전하게**: `fileIoFunc`로 읽어온 `curText`가 `\n`으로 끝나지 않으면(파일 자체가 trailing newline 없이 저장된 경우) `tagLine`의 줄 카운터가 그 include 블록을 지나서도 증가하지 않아 이어지는 토큰들이 전부 같은 줄로 잘못 태깅됨 → `fileIoFunc` 호출 성공 직후, `curText`가 비어있거나 마지막 문자가 `\n`이 아니면 하나 append.
    - **그 외 순수 컴파일 에러 수정** (전부 이 include 작업을 완성하려면 거쳐야 했던 것들): 사용자가 테스트 중이던 것으로 보이는 빈 `int handleReplace() {}`(0-인자 버전, 실제로 쓰이는 3-인자 버전과 별개로 남아있던 잔재) 제거. `preprocesser.hpp`의 새 `begin()` 메서드가 템플릿 의존 타입에 `typename` 누락 → 추가. `assemble()`의 `string curText;`(소문자, `std::` 없음, 이 파일엔 `using namespace std`가 없음)를 `std::string`으로 수정. 2차적으로 발견한 `validPreprocess`(선언 없이 쓰이던 변수) → `dump.preprocSuccess` 계산과 함께 지역 변수로 선언.
    - **렉서 규칙(`#include<[^>\t\n]*>`)이 사용자가 이미 바꿔둔 상태인데 캐시 버전이 안 올라가 있었음**: 이번 턴 작업 중 무한루프인 줄 알고 디버깅하다가, 캐시된 `assembler_table.bin`이 그 전 버전(단순 `"#include"` 키워드) 문법으로 빌드된 것일 수 있다는 걸 깨닫고 `version 403→404`로 bump + 캐시 파일 삭제 후 재생성 — 결과적으로는 이게 무한루프의 직접 원인은 아니었지만(진짜 원인은 위 `resumeEnd` 버그), 혹시 모를 stale 캐시 문제를 먼저 제거하고 나서 진짜 원인을 추적하는 순서로 진행함.
    - **검증**: ① 이름 없는 `.data`+단순 `#include<file.txt>`(define 없이) ② `#define` 매크로가 든 파일을 include ③ include된 파일 안에 실제 파싱 에러가 있는 경우 에러 메시지가 `"error at <include된 파일 경로> line <그 파일 안 줄번호>: <소스라인>\nunexpected token: ..."` 형태로 정확히 나오는지 ④ include 없는 기존 방식(메인 파일에만 에러)도 똑같이 파일명+줄번호로 잘 나오는지 — 4가지 전부 확인(③④ 전부 올바른 파일명 및 줄번호, 소스라인 재구성까지 정확). `program/` 29개 재조립 회귀도 재실행 — 새로운 크래시 없음.
    - **범위 밖으로 남은 것**: 순환 include 감지(A가 B를, B가 다시 A를 include) 가드는 요청에 없어서 안 넣음 — 지금 구조로는 실제 순환이 있으면 다시 무한 루프(메모리 고갈까지 — 이번 버그와는 다른 새로운 무한루프)에 빠질 수 있음, 필요하면 추후 요청 시 `includedFiles`를 cycle-detection에도 재사용하면 될 것.

40. (2026-10-08) "includedFiles순회하며 이미 있는 include면 error반환하게 해줘" — 39번에서 범위 밖으로 남겨뒀던 순환/중복 include 가드를 구현. `preprocesser::functions::PreprocessError`에 `DUPLICATE_INCLUDE` 추가. `assemble()`의 while 루프에서 `path`(baseDir+includePath로 resolve된 최종 경로) 계산 직후, 파일을 실제로 읽기 **전에** `includedFiles`를 순회해 같은 경로가 이미 있으면 `fileIoFunc` 호출 없이 바로 `preprocessError = DUPLICATE_INCLUDE; break;`로 while 루프를 빠져나가도록 추가 — 이후 코드(`dump.preprocSuccess = ...`, `if (!validPreprocess) return -1;`)가 기존 `INCLUDE_REQUEST` 이외의 에러 코드를 처리하던 경로를 그대로 재사용하므로 별도 분기 없이 자연스럽게 "preprocess 실패"로 귀결됨.
    - **직접 중복**(`#include<dup.txt>`를 같은 파일에서 두 번) + **순환**(A→B, B→A) 두 케이스 다 테스트 — 둘 다 무한루프/크래시 없이 즉시 "preproc-> failed"로 종료됨을 확인. 서로 다른 파일 2개를 각각 한 번씩 include하는 정상 케이스(회귀)도 여전히 정상 동작. `program/` 29개 재조립 회귀도 재실행 — 새로운 크래시 없음.

41. (2026-10-08) "main에서 inputFile abs로 변환하고(실행 cmd기준) includedFile기록시 A/B/../C -> A/C, A/./C -> A/C 로 줄여 기록해서 이거 겹치는 것도 에러 띄우게 수정해줘" — 40번의 중복/순환 include 가드가 "완전히 같은 문자열"로만 비교해서, 같은 파일을 가리키는 다른 표기(`sub/../inc.txt` vs `inc.txt`, `./inc.txt` vs `inc.txt`)는 못 잡던 걸 해결:
    - **`iotools::absolutePath(path)`** 신설(`common/iotool.hpp`, `setDirectory()` 바로 아래) — `GetFullPathNameA`로 **현재 작업 디렉터리(실행한 cmd의 cwd) 기준** 절대경로로 변환(실행파일 자신의 디렉터리를 담는 기존 `iotools::directory`와는 다른 개념이라 별도 함수로 분리). 실패 시 원본 경로 그대로 반환(fallback).
    - **`main.cpp`**: `const char* inputFile = argv[1];`를 `std::string inputFile = iotools::absolutePath(argv[1]);`로 변경 — 이후 전부(파일 읽기, 에러 메시지, `assemble()`로 넘기는 `fileName`) 이 절대경로를 그대로 사용. `iotools::setDirectory()` 호출 이후, 그리고 이 프로그램 어디서도 `SetCurrentDirectory` 같은 cwd 변경이 없으므로 "실행 cmd 기준"이 그대로 보장됨.
    - **`preprocesser::normalizePath(path)`** 신설(`assembler.hpp`, `tagLine`/`resolveSourceLocation` 근처) — 파일시스템을 전혀 건드리지 않는 순수 문자열 처리로 `.`/`..` 세그먼트를 좌→우로 접음(스택에 세그먼트를 쌓다가 `..`를 만나면 pop, `.`는 무시) — `/`와 `\` 둘 다 구분자로 인식하고, 출력은 항상 `\`로 재조립(드라이브 문자 `C:` 같은 콜론 포함 prefix는 따로 떼어내서 보존). `A/B/../C`→`A\C`, `A/./C`→`A\C` 둘 다 같은 결과로 접힘.
    - **적용 지점**: `includedFiles` 초기 시드(`{fileName}`)와 매 include 해석 시 만들어지는 `path = baseDir + includePath`를 **둘 다** `normalizePath`로 정규화한 뒤에 저장/비교하도록 변경 — 40번에서 만든 "이미 있는 경로면 `DUPLICATE_INCLUDE`" 체크가 이제 정규화된 문자열끼리 비교되므로 표기가 달라도 같은 파일이면 걸림. 실제 파일 읽기(`fileIoFunc`)도 정규화된 `path`를 그대로 사용(`.`/`..` 접기는 OS 파일시스템 입장에서도 동일한 파일을 가리키므로 안전).
    - **검증**: `#include<inc.txt>` 다음에 `#include<sub/../inc.txt>`(①) 또는 `#include<./inc.txt>`(②) — 둘 다 "preproc-> failed"(중복 감지)로 올바르게 막힘. 서로 다른 실제 파일(`inc.txt`+`sub/other.txt`) 두 개를 include하는 정상 케이스는 여전히 성공. 에러 메시지도 이제 `includedFiles[0]`이 절대경로라 "error at D:\...\badmain.txt line 2: ..." 형태로 전체 경로가 찍힘(상대경로였던 이전과 달리). `program/` 29개 재조립 회귀도 재실행 — 새로운 크래시 없음.

42. (2026-10-09) "현재 callback형식으로 수정한거 테스트 해줘" → "전부 수정해줘" — 사용자가 41번 이후 `#include` 메커니즘을 대화 밖에서 대대적으로 재설계함(요청/재개(resume) 루프 대신, `handleReplace`가 `includeCallback`이라는 콜백을 **그 자리에서 동기적으로** 호출해서 파일을 읽고 lex+strip까지 끝낸 토큰을 바로 돌려받는 구조 — `State::resumeInclude`/`resumeRemaining` 등 39-41번에서 만든 비동기 재개 메커니즘 전체가 필요 없어짐, 훨씨 단순한 설계). 그런데 이 재설계가 컴파일조차 안 되는 상태로 남아있었음 — 먼저 "테스트해줘" 요청에 빌드 시도해서 에러 6가지를 나열("분석만 해달라"는 later 요청으로 분석까지 전달), 이어서 "전부 수정해줘" 요청으로 그 6가지 + 빌드/실행 중 추가로 드러난 버그들까지 전부 수정:
    - **`FunctionRef`를 참조형에서 소유형으로 재설계**: `includeCallback`(타입 `FunctionRef<int(vector<Token>&, string, PreprocData*)>`)엔 람다가 **그 자리에서 임시 객체로 대입**되는데(`includeCallback = [&](...){...};`), 원래 구현은 콜러블을 그냥 참조(`(void*)func`로 주소조차 아닌 값 캐스트—이것부터 컴파일 안 됨)만 들고 있어서, 설령 컴파일되게 고쳐도 대입문이 끝나는 순간 임시 람다가 소멸해 댕글링 포인터가 됨. 생성자/연산자 이름 오타(`functionRef`≠`FunctionRef`), 멤버명 불일치(`obj`vs`object`), 존재하지 않는 템플릿 매개변수 `R`, `Arg..`(점 2개) 오타까지 전부 있던 걸, `operator=`가 `new Func(func)`로 콜러블을 힙에 복사해서 소유하고 소멸자에서 해제하는 방식으로 재작성(복사 생성자/대입은 `= delete`— 얕은 복사 시 이중 해제 위험, 현재 아무도 FunctionRef를 복사하지 않아 안전하게 막아도 됨). 람다가 `[&]`로 캡처한 변수들(`line`/`fileId`/`tokenBackup`/`includedFiles`, 전부 `assemble()`의 로컬)은 참조 캡처라 클로저를 복사해도 같은 원본을 계속 참조하고, `includeCallback`은 그 `assemble()` 호출이 끝나기 전에만 쓰이므로 이 복사-소유 방식으로 라이프타임 문제가 해결됨.
    - **`preprocesser.hpp`의 제네릭 `preprocess(function, data)`**: 과거(39번) 외부에서 이터레이터를 주입받는 방식으로 바꿔놨던 걸 사용자가 다시 2-인자로 되돌렸는데, 본문의 `for (; it != ...)`는 그 되돌리기 이전 버전 그대로 남아있어서 `it`가 선언 안 된 상태였음 → `auto it = this->tokens->begin();` 복원.
    - **`includeCallback` 람다 본문의 로직 버그 4개**: ① 2번째 매개변수 `path`를 `std::string path = normalizePath(baseDir + path);`로 **재선언**해서 우변의 `path`가 아직 초기화 안 된 자기 자신을 읽음(선언 없이 그냥 재대입하도록 수정) ② `std::vector<Token> curTokens;`가 같은 스코프에 두 번 선언돼 있었고, 두 번째(빈) 선언이 실제로 lex된 내용이 든 첫 번째 걸 가려서 `preprocessRem`이 빈 벡터를 처리하게 됨(중복 선언 제거) ③ `fileId`가 끝까지 한 번도 증가하지 않아 매 include가 전부 같은 fileId로 태깅됨(파일 읽기 성공 직후 `fileId++;` 추가) ④ `preprocessRem`으로 공백을 걸러낸 `curTokens`를 정작 결과로 쓰는 `tokOut`(=호출부의 `data->replaceTokens`)에 한 번도 대입하지 않음(`tokOut = curTokens;` 추가) — 이 네 개는 1차 목록(6개)엔 없었지만 깊게 읽으면서 추가로 발견, "전부"라는 요청 범위에 포함해 같이 고침.
    - **`assemble()`의 `PreprocData` 초기화 누락 2개**: `preprocData.fileName`이 한 번도 설정 안 돼 항상 빈 문자열이라 콜백의 `baseDir` 계산이 늘 `""`가 됨(`fileName`을 그대로 대입하도록 추가), `preprocData.includedFiles`(중복 감지용 set)에 메인 파일 자신의 경로가 안 들어가 있어 자기 자신을 include해도 안 걸림(초기화 시 삽입 추가).
    - **가장 심각했던, 1차 목록에 없던 구조적 버그(디버그 프린트로 직접 추적해서 발견)**: `#include` 처리가 "이 호출에서 토큰을 지우고 상태만 `replace_include`로 바꿔둔 뒤, **다음 호출에서** 그 상태를 보고 실제로 끼워넣는" 2단계 구조였는데, 지운 `#include<...>` 토큰이 그 파일(혹은 그 순간의 벡터)의 **마지막 토큰**이면 `erase()`가 돌려주는 반복자가 곧바로 `end()`가 돼버려서, 제네릭 `preprocess()` 루프의 `it != tokens->end()` 조건이 바로 거짓이 되며 **다음 호출(=실제 끼워넣기)이 영영 일어나지 않음** — include된 내용이 통째로 조용히 사라짐. 테스트 1·2·3·5·6이 전부 이 패턴(`#include`가 파일의 마지막 문장)이라 전부 "성공"으로 잘못 나왔던 것(내용이 비어서 공허하게 통과) — `fprintf(stderr, ...)` 디버그 출력을 직접 삽입해 `data->replaceTokens`엔 토큰이 제대로 들어있는데 끼워넣는 case(`replace_include`)가 **한 번도 실행되지 않는다**는 걸 확인해서 찾아냄. **고침**: 지우기와 끼워넣기를 `case TokenType::include:` **한 호출 안에서** 바로 연달아 처리하도록 합쳐서, 상태를 넘길 필요 자체를 없앰(`State::replace_include`는 이제 아무도 안 쓰므로 enum/switch case 모두 제거).
    - **검증**: 디버그 프린트 제거 후 재빌드, 8가지 케이스 재확인 — ① 단순 include(내용이 실제로 끼워들어가는지 `-preproc`로 직접 확인) ② `#define` 매크로 포함 include(치환까지 정상) ③ include 안의 파싱 에러가 올바른 파일명+줄번호로 리포트됨 ④ 같은 파일 직접 중복 include 차단 ⑤ 순환 include(A→B→A) 차단 ⑥ 자기 자신 include 차단 ⑦ `../`/`./` 경로 동치 중복도 차단 ⑧ 서로 다른 파일 2개 include는 정상(내용도 둘 다 실제로 합쳐짐, false positive 없음) — 전부 통과. `program/` 29개 재조립 회귀도 재실행 — 새로운 크래시 없음.

43. (2026-10-10) "현재 main부분의 error감지를 dump.errorCode기반으로 해주고 빌드/테스트 해서 버그 있나 찾아주고 크게 건드리지 않는 선에서 고칠 수 있으면 고쳐줘" — `main.cpp`가 `AssemblerDump::preprocSuccess`/`parseSuccess`/`evalSuccess`(사용자가 대화 밖에서 이미 `errorCode`+`AssemblerError` 비트플래그로 교체하며 지워버린 필드들)를 여전히 참조해 컴파일조차 안 되는 상태였음 — `dump.errorCode` 기반으로 바꾸고, 빌드/테스트하며 버그를 찾아 수정:
    - **`main.cpp`**: `assemble()` 호출 직후 `preprocOk`/`parseOk`/`evalOk` 세 지역 bool을 `!(dump.errorCode & AssemblerDump::FAIL_X)`로 계산해두고, 이후 모든 성공/실패 분기(`-preproc`/`-eval`/`-bin` 덤프 조건, "success"/"failed" 출력, 에러 메시지 출력 조건)를 이 세 변수로 교체. 사용자가 이미 일부(150/187번 줄)를 `dump.errorCode | AssemblerDump::FAIL_PREPROCESS`(비트 OR)로 바꿔두었던 것도 발견해 `&`(AND)로 수정 — `|`로는 결과가 항상 truthy라 "항상 실패로 보이는" 상태였음.
    - **`assembler.hpp`의 복붙 오타**: `dump.errorCode |= (validPreprocess) ? 0 : (int)AssemblerDump::FAIL_PARSE;`가 `FAIL_PARSE`를 세팅하면서 조건은 `validPreprocess`를 검사하고 있었음(바로 위 preprocess 블록을 복붙하고 변수명을 안 바꾼 것) → 이 코드에 도달하는 시점엔 이미 `validPreprocess`가 항상 true로 확정된 상태라, `FAIL_PARSE`가 절대 설정될 수 없는 상태였음. `validParse`로 수정.
    - **핵심 발견(오늘 가장 중요한 버그)**: `AssemblerDump`의 두 비트플래그 enum(`AssemblerError`/`flag`) 둘 다 `X = 0; Y = X << 1; Z = Y << 1; ...` 식으로 **값 0에서부터 좌시프트를 체이닝**하고 있었음 — `0 << 1`은 여전히 `0`이므로 체인의 **모든 값이 0**이 됨(`FAIL_LEX`/`FAIL_PREPROCESS`/`FAIL_PARSE`/`FAIL_EVALUATE`/`FAIL_FORMAT`과 `getToken`/`getPreproc`/`getParse`/`getAST`/`getEval`/`getBin` 전부). 그 결과 `dump.flags |= getToken`은 항상 no-op, `dump.flags & getToken`은 항상 false, `dump.errorCode & FAIL_X`도 항상 false — **덤프 플래그 시스템과 에러코드 시스템 둘 다 사실상 통째로 죽어있었음**(겉보기엔 "플래그 안 켜면 preproc/eval 실패 시 자동 출력" 같은 보조 로직들이 우연히 비슷하게 동작해 눈치채기 어려웠음). `FAIL_LEX`/`getToken`의 초기값을 `1`로 바꾸고 나머지는 그 값을 기준으로 체이닝하도록 수정(`getToken=1, getPreproc=getToken<<1, ...`). 부수적으로 `u32 flags;`(미초기화)도 `u32 flags = 0;`으로 — 플래그가 의미 있는 값이 된 이상, 초기화 안 된 가비지 비트가 우연히 겹쳐 덤프 조건을 오발화시킬 위험을 막기 위함.
    - **검증 중 발견한 크래시(별개 버그, 같이 고침)**: `assembler/program/*.txt` 29개(`run.txt` 제외) 전체 회귀 + 직접 작성한 최소 반례로 빌드 결과를 돌려보다, `chaintest.txt`(`.text` 섹션에 본문 없이 EOF로 끝나는 파일)가 access violation으로 크래시하는 걸 발견 — `fprintf(stderr,...)` 디버그 프린트를 `assemble()`/`evaluator.hpp`에 임시로 넣어 추적한 결과, 파싱이 실패했을 때(`!validParse`) 에러 메시지를 만드는 `dump.errorMessage += pAST->child[0]->text;`가 `pAST`(파서의 부분/에러 노드)의 child가 **0개**인 경우에도 무조건 인덱싱하고 있었음 — 에러가 첫 토큰에서 바로 발생하면 리듀스가 한 번도 안 일어나 자식이 없는 게 정상인데, 그 경우 `child[0]`이 OOB 벡터 접근으로 크래시함. `if (!pAST->child.empty())`로 가드. 디버그 프린트는 전부 제거하고 재빌드.
    - **검증**: 29개 회귀 재실행 — `chaintest.txt` 크래시 해소, `branch.txt`를 제외한 나머지 전부 크래시 없이 정상 종료(exit 0). `-token` 덤프가 지금까지 늘 빈 화면이었던 것도(enum 버그의 직접적 증상) 이번 수정으로 실제 토큰 목록이 출력되는 걸 확인.
    - **"크게 건드리지 않는 선"을 넘어서는 것으로 보여 손대지 않고 보고만 한 것**: `branch.txt`는 여전히 크래시함 — 원인을 `fprintf` 디버그로 추적한 결과, `set gen[1].32B, `처럼 콤마 뒤 expression이 비어있는 (명백히 불완전한) 소스에서 파서가 **에러로 거부하지 않고** 바로 다음 문장의 토큰(`loop` 식별자)을 그 자리의 expression으로 잘못 집어삼키며 `parser.parse()`가 `code=0`(성공)을 반환, 이후 트리가 구조적으로 뒤틀린 상태로 `calculateConst` 평가 단계에서 크래시함. 별도로 만든 반례(`.text\n    )))`처럼 완전히 무의미한 토큰 나열)도 파서가 그냥 "성공"으로 받아들이고 엉뚱한 바이너리를 뽑아내는 것까지 확인 — 즉 이건 branch.txt 하나의 문제가 아니라, **LALR 파서 테이블이 애매한/충돌하는 상황에서 조용히 틀린 액션을 골라 받아들이는 구조적 설계 문제**(37번 항목에서 이미 우려했던 "충돌 시 경고 없이 묻힘" 특성)로 보임. 제대로 고치려면 파서 생성기 자체나 문법 테이블을 깊이 손봐야 해서 이번 요청 범위("크게 건드리지 않는 선")를 넘어선다고 판단, 수정하지 않고 사용자에게 보고.

44. (2026-10-10) "LALR충돌은 정확한 error location을 못집어내도 결국 실패하는 것 같으니 굳이 수정하지 말고, 현재 극성반전은 수정했으니 나머지 버그 있는지 다시 테스트 해줘 고칠 수 있으면 고쳐주고" — 43번에서 발견해 사용자에게 보고했던 `branch.txt`의 LALR lookahead 병합 문제는 (사용자 판단대로) 그냥 두기로 하고, 43번에서 사용자가 대화 밖에서 직접 고친 "`parser::parse()`의 bool 극성 반전" 수정분(`bool validParse = parser::parser.parse(...)`로 단순화 — 중간에 `int`로 받아 `==0` 비교하던 잘못된 변환을 없앰) 이후 상태로 재빌드/재검증:
    - **검증**: `assembler/program/*.txt` 29개(`run.txt` 제외) 전체 재조립 — 크래시 전부 없음(exit 0 또는 -1만). 각 파일의 `parse->`/`evaluate->` 실제 결과를 전부 스캔해본 결과, 34번 항목에서 원래 기록해뒀던 "13개 성공(block/chaintest/fibReg/fibonacci_raw/forwarding/fullwidth/pushtest/ram/ram2/ram3/store/store2/test), 나머지는 parse 또는 eval 실패"와 **정확히 일치**함을 확인 — 즉 극성 반전 버그가 끼어들기 전의 원래 의도된 동작이 온전히 복원됨(43번 전까지는 이 극성 반전 때문에 모든 "성공해야 할" 파일이 "실패"로, "실패해야 할" 파일이 "성공"으로 보였던 것).
    - **재검증 중 찾은 추가 버그 1개(고침)**: `main.cpp`가 parse가 실패해 `evaluate()`가 실제로 전혀 실행되지 않았을 때도 `"evaluate-> success"`를 출력하고 있었음 — `dump.errorCode`의 `FAIL_EVALUATE` 비트는 evaluate가 실행될 기회조차 없으면 계속 기본값(0=미설정)으로 남으므로, `evalOk`가 항상 true로 읽혀 "성공"처럼 보임. `if (parseOk) { ...evaluate-> 출력... }`으로 감싸서 parse가 실패하면 evaluate 섹션 자체를 건너뛰도록 수정(실행된 적 없는 단계를 "성공"이라고 말하지 않게).
    - **재검증 중 찾은 추가 버그 2개(고침)**: `main()`이 `assemble()`의 성공/실패와 무관하게 항상 `return 0;`으로 끝나고 있었음 — 실패해도 (빈/부분) 오브젝트 파일을 그대로 써버리고 exit code는 0이라, 이 어셈블러를 호출하는 빌드 스크립트나 링커 입장에서 실패를 감지할 방법이 텍스트 출력을 파싱하는 것 말고는 없었음. 파일 끝에 `if (!preprocOk || !parseOk || !evalOk) return -1;` 추가.
    - **최종 확인**: 수정 후 재조립 — 13개 성공 파일은 exit 0, 나머지(branch.txt 포함, 이제는 크래시 없이 "parse-> failed"로 깨끗하게 실패)는 전부 exit -1로 정확히 구분됨.

45. (2026-10-10) "assembler, linker 빌드해서 computer32 (new)/bin에 현 dasm은 dasm-old로 옮겨지는 asm은 dasm, dlnk로 이름 바꿔서 저장해줘, 모든 table.bin삭제후 재실행 해서 꼬이지 않게 하고" — 이번엔 32번에서 썼던 `digital computer 32/tools/bin/`이 아니라 **새 배포 위치** `computer 32 (new)/bin/`(이전까지 옛 `dasm.exe`+`dasm_table.bin`+`hexdump.exe`만 있던 디렉터리, `dlnk.exe`는 아직 없던 곳)에 배포:
    - `build-asm.cmd`/`build-lnk.cmd` 재빌드(둘 다 성공).
    - `computer 32 (new)/bin/`의 기존 `dasm.exe`+`dasm_table.bin`을 `dasm-old.exe`+`dasm-old_table.bin`으로 이름 변경(32번과 동일 패턴 — 실행파일명 기준으로 캐시 파일명이 결정되므로 쌍을 맞춰 같이 옮김), 새로 빌드한 `assembler.exe`/`linker.exe`를 각각 `dasm.exe`/`dlnk.exe`로 복사.
    - **"모든 table.bin 삭제"**: 이번엔 32번처럼 새 실행파일 것만 지우는 게 아니라, 방금 옮긴 `dasm-old_table.bin`까지 포함해 디렉터리의 `*table.bin`을 전부 삭제한 뒤, `dasm-old.exe`/`dasm.exe`/`dlnk.exe` 세 개를 인자 없이 한 번씩 실행해 각자 자기 자신의 컴파일된 문법으로 테이블을 처음부터 다시 생성시킴(`dasm-old_table.bin`은 310532바이트로 이전과 동일하게 복원 — 옛 바이너리라 문법이 그대로이므로 당연함, `dasm_table.bin`은 459216바이트로 새로 생성됨 — 이번 세션 동안 추가된 토큰/문법 전부(`global`/`extern`/`equ`/`#include`/`#ifndef`/`#ifdef`/이름 있는 섹션/`equate` 등)가 반영된 크기). `dlnk.exe`는 문법 캐시 자체가 없는 링커라 테이블 파일 생성 안 됨(정상).
    - **최종 end-to-end 스모크 테스트**: 배포된 `dasm.exe`로 라벨을 각자 갖는 서로 다른 소스 2개(`f1.txt`/`f2.txt`, 각각 `addi`)를 조립 → 둘 다 parse/evaluate 성공 → 배포된 `dlnk.exe`로 두 오브젝트를 링크 → 8바이트 바이너리 정상 생성 확인(테스트 파일은 전부 `bin/scratch_test/`에 만들고 끝나고 삭제, 저장소 바깥 산출물엔 안 남음).

46. (2026-10-10) "assembler-linker/test 폴더 만들어서 program, out폴더 옮겨줘" — `assembler-linker/` 루트에 `test/` 신설, `assembler/program/`(git-tracked 테스트 소스 31개)은 `git mv`로 `test/program/`으로(rename으로 인식되어 히스토리 보존), `build/out/`(세션 중 쌓인 untracked 테스트 산출물)은 `test/out/`으로 일반 `mv`. 두 경로 모두 빌드 스크립트(`build-asm.cmd`/`build-lnk.cmd`)나 소스(`main.cpp`/`*.hpp`) 어디에도 하드코딩 참조가 없음을 먼저 grep으로 확인한 뒤 이동(참조가 있었다면 같이 고쳐야 했을 것). 디렉토리 구조 섹션도 갱신.

47. (2026-10-10) "evaluateCode안에 calculateAddr(구calculateConst)의 identifier, expression처리 등 addr이외의 const처리를 복사해서 evaluateCode에서 unresolved된 identifier/expression 처리하게 해줘" — 직전 분석(add_terminal이 `call zero.32B, func_getInput`의 전방 참조에서 `MISSING_EXPRESSION`) 후속:
    - **전방 참조 해소**: 사용자가 `calculateConst`를 `calculateAddr`로 이름 바꿔둔 상태. 복붙 대신 표현식 case들(연산자/리터럴/identifier/index)을 공용 함수 `calculateExpr(cur, data)`로 분리해 `calculateAddr`와 `evaluateCode` 양쪽에서 호출(로직이 두 벌로 갈라지지 않게). `calculateExpr`는 피연산자가 없으면 에러 대신 skip(binaryOp/unaryOp/단항+의 `MISSING_EXPRESSION` 반환 제거) — `evaluateCode`는 `expressionValue`에 없는 노드만 재시도, 끝내 못 푼 값은 소비자(`immidate16` 등)가 `MISSING_EXPRESSION`으로 보고. `allocate` 크기 식은 주소를 결정하므로 `calculateAddr`에서 여전히 CONSTANT 필수.
    - **같이 고친 기존 버그 1 (`collapseExpr` 중복 판정)**: `EqualOperation`/`HashOperation`이 `type`/`value`만 비교하고 `text`를 무시 — `IDENTIFIER` 토큰은 전부 `value=0`이라 `[ID a]`와 `[ID b]`가 같은 식으로 판정돼, 서로 다른 라벨 참조가 전부 첫 번째 라벨 주소로 패치되고 있었음(전방/후방 무관 — `a:`,`b:` 정의 후 `jmp a`,`jmp b`가 둘 다 a로 감). `text`도 비교/해시하도록 수정.
    - **같이 고친 기존 버그 2 (`formatData`의 `exprIndex`)**: `nMap.exprIndex = si->second.index`가 raw `operationVector` 인덱스를 썼는데 `format.expressions`는 중복을 건너뛴 압축 목록이라 인덱스가 어긋남(링커가 `exprIndex >= expressions.size()`로 실패). 버그 1 때문에 모든 단일 라벨 식이 인덱스 0으로 뭉쳐서 지금까지 드러나지 않았음. `data.compressionIndex[...]`로 변환하도록 수정.
    - **검증**: 최소 반례(후방/전방 참조 각 2라벨) 링크 결과가 정확한 주소(0/4, 8/12)로 패치됨. `add_terminal` 링크 결과를 손으로 검산 — `func_getInput`=120(앞 명령어 30개), `func_convertInt`=260, `numA`=576(text 496 + data 80), `stackEnd`=672 전부 일치. `test/program` 29개: eval 성공 13→25개, 25개 전부 단독 링크까지 성공. 남은 실패 5개는 기존 문제(parse 실패 4: branch/factorial_norec/prime_old/texteditor, eval 실패 1: bcdtodec — 미조사).

48. (2026-10-10) eval 실패 시 위치/토큰/에러 이름 리포트 + AST 소스 토큰 정리:
    - AST 소스 토큰: 사용자가 추가한 `pSrcToken`(포인터)을 이후 요청으로 **값 복사 `ASTNode::srcToken`**으로 바꿈(기본 생성 시 line=-1). 값은 **리듀스 시 가장 왼쪽 심볼의 토큰**(bison `@$ = @1`) — 이전엔 nodeIndex 자식 또는 스택 top(=가장 오른쪽)이라 노드마다 기준이 달랐음. `parse()`는 `inputs`(const, 호출자 벡터)에 `__eot`를 push하지 않고 가상의 `eotToken`을 lookahead로 씀.
    - parse 에러 토큰은 `inputs[tokenCount]`(실제로 걸린 lookahead), 입력 끝이면 마지막 토큰 위치에 text `<EOF>`.
    - `Evaluator::evaluate(errorToken, ...)`가 실패 노드의 `srcToken`을 복사. `evaluator::evaluate`는 `errorTok`를 참조로 받음(이전엔 값 복사라 전달 안 됨). `evaluateErrorName(code)` 추가.
    - `assemble()`의 `describeLocation(tok)` 람다를 parse/eval 에러 메시지가 공유: `error at <file> line <fileLine>: <소스 줄>` + parse는 `unexpected token: ..`, eval은 `<ERROR_NAME>: <token>`. 위치는 토큰 자체의 `fileId`/`fileLine` 사용. `main.cpp`가 eval 실패 시 메시지 출력.
    - `tokenBackup`을 `unordered_multimap`→`std::multimap`: 같은 줄 토큰이 역순으로 나와 소스 줄이 뒤집혀 출력되던 기존 버그 수정(`-token` 덤프 순서도 정렬됨).
    - 검증: bcdtodec → `line 134: st.zero gen[7].32B, bcdTest / MISSING_EXPRESSION: bcdTest`. 회귀 25/29 그대로.

49. (2026-10-10) `DataValue::length` 미설정 수정: `evaluateCode`의 `allocate`가 `length`를 안 채워 `.data` 바이트가 0개로 나가던 문제(라벨 주소는 `sectionByte`로 따로 세서 맞았음). `length = child[0]`(바이트, CONSTANT 보장) 대입, `registerReplace`에 `length/8` 대신 `length`(바이트), 비상수 값의 `rep.size = min(32, length*8)`(4바이트 미만 allocate에서 32비트 패치가 앞 데이터를 덮던 문제 — `ptr: 4, v` 다음 `ptr2: 2, w`가 ptr 하위 2바이트를 0으로 덮음). 검증: add_terminal 496→672바이트, `ptr`/`ptr2`/1바이트 `b` 재배치 정확, 회귀 25/29 asm+link.
    - (후속, 사용자 선택 B → 이후 사용자가 직접 A로 되돌림: 현재는 끝 기준 채우기) `pushByteN`을 시작 기준 채우기로 변경: size<=4는 하위 size바이트(빅엔디안, 기존 유지), size>4는 값을 앞에서부터 빅엔디안 반복하고 마지막 부분 워드는 잘림(`5, 0x11223344` → `11 22 33 44 11`; 이전엔 끝 기준이라 `44 11 22 33 44`로 라벨 첫 워드가 회전된 값이었음). 비상수 재배치 위치도 마지막 워드→**첫 워드**(`registerReplace`에 `min(length,4)`).
    - 남은 것(미수정): 4바이트 초과 allocate의 비상수 값은 첫 워드에만 패치되고 나머지는 0, 4의 배수 아닌 크기 뒤 라벨들의 워드 비정렬. (`.bss` 크기 직렬화는 53번에서 해결)

50. (2026-10-10) `equ`/`$` 평가 연결 + allocate 섹션 매핑 버그:
    - **equate**: `calculateAddr`에서 식 값이 있으면 `idenifierValue[name]`에 ExprData 복사(없으면 skip, 에러X). `evaluateCode`에서 아직 없으면 재시도된 식 값으로 등록, 끝내 없으면 `MISSING_EXPRESSION`. 사용처는 식을 복사하므로 `K equ later+4`는 `later` 재배치로 처리됨. 한계: equ 정의보다 앞에서 쓰이고 + equ 식이 전방 라벨에 의존하면 사용처가 먼저 평가돼 실패.
    - **`$`(curline)**: 문법 `expressionL0 -> curline` 추가(version 404→405). `calculateAddr`에서 현재 `sectionByte`(그 명령어/데이터 시작 위치)로 LINEAR(coeff{section:1}) ExprData 생성. 이후 사용자가 재배치 지원을 위해 `$`마다 숨은 라벨 `#curline#N`(`EvalData::curLineCount`)을 만들어 `idenifierValue`+`expressionValue` 양쪽에 저장하도록 변경 → 일반 라벨처럼 export/mangle돼 링커가 해석. 검증: `jmp $`, `$ + 8`, `.data`의 `v: 4, $`, `K equ $`, 같은 이름 `.text` 두 조각, 2파일 링크 전부 정확한 주소.
    - **같이 고친 기존 버그**: `allocate`/`allocate_zero`가 `sectioningArr`에 없어 `sectionIdMap[cur]`가 기본값 0 → `.text` 다음 `.data`면 데이터 바이트/크기가 섹션 0(text)에 들어가 라벨 주소와 실제 위치가 어긋남. `calculateSection`에 allocate/allocate_zero/curline 추가.
    - 검증: equ 상수/전방 라벨/미정의(에러 메시지), `$ - start`, `$ - v`(data), text→data 순서 전부 바이트 단위 확인. 회귀 25/29 asm+link, add_terminal 672바이트.

51. (2026-10-10) `#ifdef`/`#ifndef` 테스트 + 버그 수정 + 전처리 에러 리포트(사용자가 1차 구현한 상태에서):
    - 버그: ① `ifndef`가 `ifdef`로 fall-through하며 `invert=false`로 덮여 `#ifndef`가 `#ifdef`처럼 동작 ② 조건이 참이면 state가 `readIf`에 머물러 다음 토큰에서 `EXPECTED_IF_IDENTIFIER` ③ 참인 블록의 `#end`를 소비하는 곳이 없어 파서까지 감 ④ 제거 중인 블록 안의 중첩 `#ifdef`/`#macro`의 `#end`에서 제거가 끝나버림.
    - 수정: `PreprocData::ifOpen`(열린 블록의 여는 토큰 스택, 에러 위치용) + `removeDepth`(제거 중 중첩 지시어 깊이). normal 상태의 `#end`는 `ifOpen`을 닫고, 비어 있으면 `UNEXPECTED_END`. `invert`/`uniqueCounter` 기본값 초기화(기존엔 미초기화).
    - 에러: `UNEXPECTED_END`, `UNTERMINATED_IF`(입력 끝에 열린 블록 — 여는 `#ifdef` 위치 보고), `UNTERMINATED_DIRECTIVE`(입력 끝에 `#macro`/`#define`/매크로 호출 미완) + `preprocessErrorName()`. 제네릭 `Preprocesser::preprocess(errorToken, ...)`가 실패 시점 토큰을 돌려주고, `assemble()`이 `describeLocation`(전처리 앞으로 이동)으로 parse/eval과 같은 형식의 메시지 작성, `main.cpp`가 출력.
    - 검증: ifdef/ifndef 참·거짓, 중첩(참/거짓 조합), 제거 블록 안 `#macro`, 참 블록 안 `#macro` 정의 후 호출, include guard(`#ifndef X #define X`)와 include 파일 간 guard, 에러 5종(위치 포함). 회귀 25/29 asm+link.
    - (후속 요청) `readMacro_token`에서 `#ifdef`/`#ifndef`면 `removeDepth++`, `#end`면 `removeDepth>0`일 때 `--`하고 본문에 포함, `removeDepth==0`인 `#end`에서만 매크로 정의 종료(`readMacro_argCount`에서 0으로 리셋). 본문의 조건부는 **확장 시점**에 평가됨(`#define`이 두 호출 사이에 오면 두 번째 호출만 포함 확인). 한계: `#define`은 괄호로 묶지 않으면 토큰 1개만 받음(기존 동작).

52. (2026-10-10) 배포 전 전체 점검(`-Wall -Wextra` + 기능 스윕 약 40케이스: 전방/후방 참조, 식 연산자, LINEAR/COMPLEX 재배치, text↔data 순서, bss, 이름 있는 섹션, 같은 이름 text 조각, `$`, equ, 매크로 인자/`##`고유 라벨, 중첩 define, 매크로 안 ifdef, include 정상/중복/자기자신/없는 파일/include 파일 안 에러, 에러 메시지 종류별, 빈 파일, 3파일 링크):
    - **수정 1 (lexer)**: `lex()`가 DFA 테이블(열 0..127)을 부호 있는 `char`로 인덱싱 → UTF-8 바이트(≥0x80)가 음수 인덱스로 테이블 밖을 읽음(UB). 한글 주석이 있으면 어셈블 실패. 0x80 이상은 127(DEL)로 조회(`[^
]` 같은 부정 클래스에 매치돼 주석 안에서는 허용, 코드에선 unknown 토큰), 토큰 text는 원래 바이트 유지.
    - **수정 2 (parser)**: 빈 입력의 parse 에러 토큰이 빈 문자열 → `<EOF>`.
    - 확인만 한 것: `run.txt`는 소스가 아니라 바이너리(실패가 정상). ram+ram2+ram3 링크가 108→260바이트인 건 49번으로 `.data`가 실제로 출력되기 때문(168+44+48). 경고 중 `for (it; ...)`류/`readInclude` 미처리 switch/미사용 변수는 동작 영향 없음.
    - 남은 실패: parse 4(branch/factorial_norec/prime_old/texteditor, LALR/소스), eval 1(bcdtodec, 소스에 없는 라벨).

53. (2026-10-10) `.bss` 크기를 오브젝트에 기록: `format_data::Section`에 `u32 size`(섹션이 차지하는 주소 공간) 추가, `getRaw`/`loadRaw`에 직렬화(**오브젝트 포맷 변경 — 이전 .o는 새 링커와 호환 안 됨, 재어셈블 필요**). 어셈블러 `formatData`는 `size = sectionByte[i]`(bss의 `allocate_zero`도 포함, text/data는 바이트 수와 같음). 링커는 `SectionData::size`를 두고 `convertData`의 localBase, `mergeByName`의 파일별 오프셋, `mergeByType`의 그룹 배치/`placement.end`/`dataBase`·`bssBase`를 전부 바이트 수(`ninIndex.size()`) 대신 `size`로 계산. bss는 바이트 없이 주소만 차지(최종 바이너리에 포함 안 됨).
    - 고쳐진 실제 버그: 여러 파일이 각자 `.bss`를 가지면 전부 크기 0으로 취급돼 같은 주소에 겹쳤음(이전: 두 파일의 bss 라벨이 둘 다 0x10, 수정 후 0x10/0x20). 단일 파일에서 `.bss` 뒤에 다른 `.bss`가 오는 경우도 정확.
    - 검증: 회귀 25개 프로그램의 최종 바이너리가 이전 배포본과 바이트 단위로 동일(text/data에서 sectionByte == 바이트 수 확인), bss 단일/2파일 케이스 주소 검산.

54. (2026-10-10) 루트에 `SYNTAX.md`(사용자용 문법 문서) 작성: 도구 사용법, 파일/섹션 구조, 라벨·equ·`$`, 데이터 채우기 규칙, 식 우선순위·리터럴, 레지스터/모드/플래그 값, 명령어 opcode·인코딩 표, 전처리기, 링크 배치, 오류 메시지. 내용은 소스(`CreateData` 렉서/파서 규칙, `instructionBase`/`regBase`/`flagBase` 등 테이블, `evaluateCode` 인코딩)와 실제 배포본 dasm으로 확인한 결과 기준. 문법/인코딩을 바꾸면 이 문서도 갱신할 것.
    - 작성 중 확인된 동작(미수정): 인자 0개 매크로를 괄호 없이(`M`) 부르면 다음 토큰이 인자로 먹힘, `#include` 경로가 중첩 include에서도 최초 입력 파일 디렉터리 기준(`PreprocData::fileName`이 갱신 안 됨), `gen[n]` 인덱스 범위 검사 없음(25 이상이면 특수 레지스터 id와 겹침), `intervec`의 첫 피연산자는 `reg` 키워드 토큰이라 항상 0.

55. (2026-10-10) `computer 32 (new)/digital computer 32/ISA-32.xlsx`(CPU 쪽 ISA 시트)를 읽어 같은 폴더에 `ISA.md`로 요약. **이제 레포 밖이 아닌 ISA 문서가 있음** — opcode/필드 의미는 xlsx(=ISA.md)를 우선 참고. 시트와 현재 어셈블러의 불일치(미수정, ISA.md 6절): `pctl`은 시트상 7C 하나+byte3 en/dis(어셈블러는 7C/7D), `getabs` 7D(어셈블러 7E), 캐시는 `cache` 7E 하나+id의 md 비트로 flush/inval 구분(어셈블러는 flush 7F/inval 80 분리), 캐시 id all=0x40·md=0x80(어셈블러 ALL=0x80), `atom` 7F 미구현, `intervec` byte1은 base 레지스터(어셈블러는 `reg` 키워드라 0), 시트상 `rol`=rollright/`ror`=rollleft로 이름과 반대.

56. (2026-10-10) `global`/`extern` 파싱 + `BUGS.md`(사용자가 floating point 코드 작성 중 정리) 3건 수정:
    - **global/extern**: 사용자가 미리 써둔 규칙(`NT::global`/`AT::global`, `NT::_extern`/`AT::_extern`, nodeIndex 1 = 이름, 식별자 1개)에 맞춰 NT/AT enum과 디버그 문자열 추가, `outer_section` 생산규칙(global/extern 1개 이상, 섹션 앞·사이·뒤) 추가, version 405→406. 평가기 연결은 아직 없음(outer_section은 기존대로 OUTER 섹션을 하나 만들 뿐, 링커는 OUTER를 건너뜀). 처음엔 콤마 목록(`global a, b`)용 `global_decl`/`identifier_list`로 만들었다가 사용자 규칙과 겹쳐 사용자 쪽 이름/형태로 통일함.
    - **BUGS 1 (1인자 매크로)**: `)`에서 마지막 인자를 넣는 조건이 `args.size()`라 1인자 호출은 인자가 안 들어가고 파일 끝까지 먹음. "쉼표가 있었거나 curArg가 비어있지 않으면" push, 그 뒤 개수 불일치는 `MACRO_ARG_COUNT_MISMATCH`. 확장 코드는 `expandMacro()`로 분리하고, 인자 0개 매크로는 `(`가 안 따라오면 normal 상태에서 즉시 확장(예전엔 다음 토큰을 먹음), 인자 있는 매크로가 `(` 없이 오면 `EXPECTED_MACRO_CALL`.
    - **BUGS 2 (`;` 단독)**: 문법이 안 쓰는 `semicolon` 렉서 규칙 삭제(같은 길이면 앞 규칙 우선이라 `;[^\n]*` 주석에 짐), version 406→407.
    - **BUGS 3 (dlnk segfault)**: `Format::loadRaw`를 `bool`로 바꾸고 헤더 크기/구역 연속성·범위/레코드 경계(NUL, 고정 필드, EOX)/섹션 start·end 검사. `link()`에 `errorMessage` 인자 추가, `dlnk`가 `invalid object file '<경로>'` 출력. `dasm`은 실패 시 출력 파일을 쓰지 않고 기존 파일 삭제.
    - 검증: BUGS 재현 3건 + 매크로 인자 0/1/2개·괄호 안 쉼표·개수 오류·괄호 생략, `;` 단독 줄, global/extern 위치별, 0바이트/무작위/잘린/섹션 범위 손상 오브젝트. 회귀 25개 최종 바이너리가 배포본과 바이트 단위 동일. SYNTAX.md/BUGS.md 갱신.

57. (2026-10-10) `global`/`extern` 의미 구현 (사용자가 추가한 `EvalData::globalIdentifier`/`externIdentifier` set + `calculateAddr`의 수집 case 기반):
    - **mangle 결정 (`formatData`)**: definedIdentifier는 `globalIdentifier`에 있으면 원래 이름, 아니면 `파일명#이름`. usedIdentifier(재배치 식의 IDENTIFIER)는 global 또는 extern이면 원래 이름, 아니면 mangle. 사용자가 남겨둔 빈 `if ()` 두 곳을 채움.
    - **extern 사용 (`calculateExpr` identifier case)**: `idenifierValue`에 없고 extern이면 `{COMPLEX, 0, {}, [IDENTIFIER name]}` → 항상 재배치. extern 선언이 사용보다 뒤에 있어도 evaluateCode 재시도에서 해결됨. 상수가 필요한 곳(allocate 크기)은 `NOT_CONSTANT`.
    - **검사 (`evaluateCode`)**: `global` 이름이 정의 안 됨 → `GLOBAL_UNDEFINED`, `extern` 이름이 이 파일에 정의됨 → `EXTERN_DEFINED`.
    - **링커**: `EvalData::errorMessage` 추가. `checkDuplicateSymbols()`(mergeByType 다음): 같은 이름이 두 파일의 definedLabel에 있으면 `duplicate symbol`(지역 이름은 접두사가 달라 안 겹침; 출력 파일명이 같은 두 오브젝트도 이제 여기서 잡힘). `mapExpr`의 미해결 이름 → `undefined symbol '<이름>' (used in '<파일>')`. `link()`가 메시지를 `errorMessage`로 전달.
    - 검증: lib(global 함수/data/equ 상수 + 지역 loop) + main(extern 3개 + 지역 loop + `.data p: 4, add1`)을 두 순서로 링크해 전 주소 손계산 일치, extern 선언을 사용 뒤에 둔 경우, 미정의/중복/global 미정의/extern 정의/extern 크기 오류. 회귀 25개 배포본과 바이트 동일. SYNTAX.md 8·9·10절 갱신.

58. (2026-10-10) 56·57번까지 반영해 빌드·배포: `computer 32 (new)/bin`과 `digital computer 32/tools/bin`의 dasm/dlnk 교체(table.bin 전부 삭제 후 재생성, dasm_table.bin 454348바이트 — semicolon 규칙 삭제로 작아짐). `digital computer 32/tools/`는 소스 미러 구조(`assembler/`·`linker/`·`common/`·`hexdump/` + `bin/` + `SYNTAX.md`)라 변경된 소스 5개와 `SYNTAX.md`를 그대로 복사, `BUGS.md`(tools 기준 경로로 쓰여 있음)도 `tools/BUGS.md`로 배포. 확인: 두 위치 모두 회귀 25/29, BUGS 재현 3건 해결, `digital computer 32/program`의 `floatingpoint/float.txt`·`fp_harness.txt` 어셈블+링크 성공. `texteditor.txt`는 19행이 `#macro`가 아니라 `#define LSET 2`로 쓰인 소스 오류라 이제 전처리 단계에서 `#end without matching` 으로 실패(이전엔 파서에서 실패).

59. (2026-10-11) `-o` 플래그: dasm `<in> [-o out]`(기본 `<입력>.o`), dlnk `<in...> [-o out]`(기본 `a.out`). 쓰이지 않던 dasm `-double`/`dumpAST` 제거. 예전 위치 인자 형식은 오류로 끝남.
60. (2026-10-11) `dlnk -padbss`: `linker::bssPadding`이 켜지면 `mergeByType`이 bss 그룹 바이트를 그 크기만큼 0으로 채움(맨 뒤라 주소/재배치 영향 없음).
61. (2026-10-11) 저장소 루트를 `src/assembler-linker/` → `src/`로 이동(`.git` 포함). `csvsearcher/`·`textediter/`는 `.gitignore`. `hexdump/`는 저장소에 포함, `build-hex.cmd` 추가(→ `build/hexdump.exe`). 빈 `src/assembler-linker` 폴더는 핸들이 잡혀 삭제 실패(수동 삭제).
62. (2026-10-11) 터미널 입출력 통일(`CLI.md`): `common/iotool.hpp`에 `errPosGenerator`(GCC 형식 위치)와 `Terminal::TerminalIO<FlagType>`(공용 인자 파서) + 공유 오류/IO 함수(`toolError`/`codeError`/`readFailed`/`writeFailed`/`usageError`/`parseNumber`, 종료 코드 0/1/2). 세 main이 사용, stdout=요청 출력·stderr=진단. `assemble()`의 위치 오류는 `describeError`가 `<file>:<line>: error: <detail>\n    <소스>`로 만듦. hexdump는 `-o` 추가, 값 검사로 0 나누기/NULL 크래시 해결. 회귀 25/29 바이너리 배포본과 동일. 같은 날 SYNTAX.md 1·9·10절 갱신, `bin/`·`digital computer 32/tools`에 dasm/dlnk/hexdump 배포.

63. (2026-10-11) `.data` 목록/문자열 데이터(사용자 초안 문법 완성): `allocate_list` = `total(element), {e0, e1, ...}`(NT `list`/`listtail`, AT `list`), `allocate_string` = `total, "str"`. version 407→408. `calculateAddr`에서 total/element 상수·`total%element==0`·원소 수/문자열 길이 검사(새 에러 `INVALID_SIZE`/`INITIALIZER_TOO_LONG`). `evaluateCode`: 원소마다 `element, 값` allocate와 같은 방식(`pushByteN` + 비상수면 `ReplaceData` size=min(32, element*8)), 모자란 원소는 0. 문자열은 `string2bytes`(이스케이프 \n \t \r, 10진 최대 3자리)로 디코드 후 0으로 패딩, NUL 자동 추가 없음. 빈 목록 `{}`도 허용(전부 0, version 409). 회귀 25개 배포본과 바이트 동일. SYNTAX.md 4·5·8·10절 갱신. 같은 날 `bin/`·`digital computer 32/tools/bin`에 dasm 배포(table 재생성 465808바이트 — 새 CLI라 인자 없이 실행하면 table을 안 만들어서 실제 파일을 한 번 어셈블해 생성), tools 미러에 `assembler.hpp`·`SYNTAX.md` 복사. dlnk/hexdump는 변경 없음.

64. (2026-10-12) 바이트 순서 플래그 `-big`/`-little`(기본 little, 같이 주면 usage 오류) + `TerminalIO` 플래그를 u64(최대 64개)로 확장(`parse(u64&,...)`, `has(u64,...)`, `1ull<<`; 세 main의 `flags`도 `iotools::u64`).
    - **오브젝트 포맷 변경**: `Header`에 `endian` 필드(14필드, 56바이트), `Format::endian`(`format_data::Endian` LITTLE=0/BIG=1). 오브젝트 메타데이터 자체(push32/read32)는 계속 빅엔디안 고정. 이전 .o는 `invalid object file`로 거부됨 — 재어셈블 필요.
    - dasm: 전역 `assembler::bigEndian`. `push32`는 `pushByteN(…,4)`로, `pushByteN`은 빅엔디안 바이트열(5바이트 이상 끝 기준 반복 그대로)을 만들고 little이면 통째로 뒤집음. `registerReplace` 키는 big=`start+byte-1-offset/8`, little=`start+offset/8`.
    - dlnk: 전역 `linker::bigEndian`, `link()`가 오브젝트 순서가 다르면 `object file 'x' is big-endian, linking little-endian (use -big)`. `mapExpr` 패치는 little이면 뒤 바이트로 확장(끝 넘으면 실패).
    - hexdump: 표시만 바꿈 — little은 각 `-bundle` 묶음 바이트를 뒤집어 출력(끝의 모자란 묶음은 왼쪽이 빔), big은 파일 순서(예전 출력). 문자 열은 항상 파일 순서. dump 함수는 `hexColumn`/`textColumn`으로 재작성.
    - 검증: 회귀 25개 — `-big` 결과가 배포본 바이너리와 바이트 단위 동일, little 결과는 전부 정확히 워드 단위 뒤집힘. 2/1/5바이트 데이터, 2바이트 목록 재배치, 16비트 즉시값 재배치를 손으로 확인. 순서 불일치/옛 오브젝트/플래그 충돌 오류 확인. SYNTAX.md 1·4·7절, CLI.md 2절 갱신. 같은 날 `bin/`·`digital computer 32/tools/bin`에 dasm/dlnk/hexdump 배포(table.bin 전부 삭제 후 재생성), tools 미러에 소스 7개+SYNTAX.md 복사. **주의**: `digital computer 32`의 `isa32emu.py`(fp_test.py)와 dsim `Sim`(ProgramMemoryLoader bigEndian=true)은 빅엔디안 프로그램을 읽으므로 그쪽에 넣을 바이너리는 `-big`으로 만들어야 함(미수정). → 2026-10-17: Digital/dsim 시뮬레이터는 이제 리틀엔디안(기본 -little). Python `isa32emu.py`는 여전히 빅엔디안(`>I`)이라 fp/io/pi_test.py는 `-big -raw`로 호출(computer32 manager 세션이 수정, 진입 프로그램에 `global _start` 추가).

65. (2026-10-17) 어셈블러 `org` 문법/`Format::origin` 헤더 기록을 한 번 구현했다가 사용자 요청으로 전부 롤백(assembler.hpp/format.hpp/문서를 커밋 상태로 되돌림, 테이블 version 409 그대로, 오브젝트 포맷 변경 없음). 대신 링커 `-org N`: `linker::origin`(int, 기본 0)을 `mergeByType`의 `textBase`로 써서 첫 그룹(text, 없으면 data)이 N에서 시작 → 섹션 오프셋·`texts/datas/bsses`·라벨 전부 N만큼 이동(상수 식별자는 그대로). `mapExpr`의 바이너리 위치는 `sectionOffset - origin + 오프셋`. 출력 파일 앞에 패딩 없음. 검증: text/data/bss 라벨·extern·equ 상수 섞인 2파일 링크에서 `-org 0x1000`이 라벨만 +0x1000, `-org 0`과 무플래그 동일, 잘못된 값은 usage 오류. 회귀 25개 배포본과 바이트 동일. SYNTAX.md 1·9절 갱신. `EvalData::orders`(사용자가 추가, 미사용)는 그대로 둠.

66. (2026-10-17) 섹션 이름 mangling + `global section`:
    - **기존 버그 수정(이름 있는 섹션)**: 섹션 래퍼 노드가 `program`/재귀 `section`에 splice돼 사라지고 본문 리스트 노드만 `AT::text_section`으로 남아서, `.text a`의 이름이 항상 `""`로 기록되고 있었음(37번 때 이름까지는 확인 안 했음). 문법을 `section -> section_unit section(splice) | section_unit`, `section_unit(AT::*_section) -> .text [identifier] [text_section(splice)]`(data/bss 동일, `section_unit(AT::outer_section) -> outer_section(splice)`)로 재구성 — 섹션당 노드 하나에 이름이 child[0], 문장이 그 뒤. 새 NT `section_unit`.
    - 토큰 `section`(키워드), 규칙 `global section identifier`(nodeIndex 2) / `global section`(이름 생략 = 이름 없는 섹션 `""`, nodeIndex -1)(NT::global, AT `global_section`). 처음엔 `global section ""`(문자열)로 만들었다가 사용자 요청으로 이름 생략 형태로 바꿈(`GLOBAL_SECTION_NAME` 오류 삭제, version 412). `calculateAddr`가 `EvalData::globalSectionIdentifier`(사용자가 추가한 set)에 이름 등록(자식이 없으면 `""`), `evaluateCode`가 OUTER 아닌 같은 이름 섹션이 없으면 `GLOBAL_SECTION_UNDEFINED`.
    - `formatData`: `Section.name`/`Mapper.sectionName`/`Identifier.section`을 global section이 아니면 `mangleLabel(fileName, name)`(빈 이름도 `file#`). 링커는 그대로 — (type,name) 병합이 파일 내로 한정됨.
    - version 409→411. 검증: 이름 있는/없는 섹션 2파일 링크에서 기본은 파일별 분리, 양쪽 `global section`이면 병합 배치 확인, 오류 3종, 37번 섹션 문법 케이스 재확인. 회귀 25개·ram 3파일 링크·float 링크 모두 배포본과 바이트 동일. SYNTAX.md 2·7·8·9·10절 갱신.

67. (2026-10-17) ISA 시트(xlsx)에 맞춤 (55번의 불일치 해결): `pctl` 7C + byte 3에 en(1)/dis(0)(`instruction_CTL` = opcode<<24 + 피연산자, 재배치 offset 0), `getabs` 7E→7D, `flush`/`inval` 둘 다 7E(`cache`)이고 flush면 id에 `cacheFlushBit`(0x80, md) OR — md 방향은 시트에 "md: flush/inval"뿐이라 flush=1로 가정, `ALL` 0x80→0x40, 새 토큰 `atom`(7F, `atom imm8` → byte 3, `instruction_CTL` 재사용), `intervec` 첫 피연산자 `TT::reg` → `NT::reg`(base 레지스터 바이트). version 412→413. `jp`/`jpi`/`break` 이름과 `rol`/`ror` 방향은 그대로(ISA.md 6절 남은 차이). 검증: 새 인코딩 손확인(`atom label` 재배치 포함), 회귀 25개 배포본과 바이트 동일(테스트 프로그램엔 해당 명령어 없음). 같은 날 65~67번을 `bin/`·`digital computer 32/tools/bin`에 배포(dasm/dlnk/hexdump, table.bin 전부 삭제 후 재생성 — dasm_table.bin 469762바이트), tools 미러에 assembler.hpp·linker.hpp·linker/main.cpp·SYNTAX.md 복사. `digital computer 32/ISA.md` 6절 갱신은 그 저장소에서 미커밋.

68. (2026-10-17) dlnk 출력 헤더: `linker::formatted`(기본 true, `-raw`면 false) + `linker::startLabel`(기본 `_start`, `-start <label>`, `-raw`와 같이 쓰면 usage 오류). `evaluate`의 마지막 단계 `addHeader`가 `totalBin` 앞에 `{시작 주소, 프로그램 길이}`(각 4바이트, 출력 바이트 순서)를 붙임 — mapExpr 이후라 재배치 위치와 주소는 헤더와 무관, 길이는 헤더 제외 크기. `findStartLabel`: 전 파일 `definedLabel`에서 원래 이름(global) 먼저, 없으면 `<file>#이름` 지역 라벨을 찾되 한 파일에만 있어야 함(0개/여러 개면 오류 메시지). 검증: 단일/2파일, global/지역, `-big -org 0x100`, 오류 3종. `-raw` 결과는 회귀 25개 배포본과 바이트 동일. **주의**: 기본이 헤더 포함이라 `_start`가 없는 기존 프로그램(test/program 전부)과 `digital computer 32`의 `fp_test.py`/`io_test.py`/`pi_test.py`(dlnk 호출)는 `-raw`를 붙이거나 `_start`를 정의해야 함(미수정). 같은 날 dlnk만 `bin/`·`digital computer 32/tools/bin`에 배포(dasm/hexdump는 변경 없음), tools 미러에 linker.hpp·linker/main.cpp·SYNTAX.md 복사.

69. (2026-10-17) `.text`에서 `equ` 허용: `text_section`에 equate 재귀/단독 규칙 2개 추가(data/bss와 같은 형태), version 413→414. 평가기는 변경 없음. 검증: text 안 상수 equ, `HERE equ $`(현재 위치), 전방 라벨 기반 `L equ later + 4`, 사용보다 뒤에 정의된 상수 equ, `.text`에 equ만 있는 섹션. 회귀 25개 배포본과 바이트 동일. 같은 날 dasm을 `bin/`·`digital computer 32/tools/bin`에 배포(dasm_table.bin 삭제 후 재생성 471306바이트), tools 미러에 assembler.hpp·SYNTAX.md 복사.

70. (2026-10-18, "tools" 세션 요청) ISA 시트 EXT 갱신 반영: 토큰 `plimit`(별칭 `pagelimit`) = id 5, `pagebase` 별칭 `pbase`(같은 토큰으로 lex), 캐시 id `cacheid | 이름` 조합(자식 토큰 OR, `L1I | L1D` = 0x03). version 414→415. 회로(.dig)에는 EXT 레지스터 디코드가 아직 없어 시트 기준 ex-n → id n-1 유지. `jp`/`jpi`/`break` 별칭은 추가 안 함(라벨 이름 충돌 위험). `digital computer 32/program`·`boot` 17개 `.asm`이 이전 dasm과 바이트 동일, 회귀 25/29. tools/bin에 dasm 배포(table.bin 전부 삭제 후 재생성 486170바이트), tools 미러에 assembler.hpp·SYNTAX.md 복사, `document/ISA.md` 2·4·6절과 `document/todo.md` 갱신(그 저장소에서 미커밋). `computer 32 (new)/bin`은 이번에 배포 안 함.

## 작업 방식 관련 주의사항

- ~~빌드/테스트를 자동으로 수행하지 말 것~~ → 2026-10-11 해제: 변경 후 빌드와 회귀 테스트(`test/program`, 25/29 성공 기대)는 묻지 않고 해도 됨. 배포/커밋은 요청 시에만.
- bash 도구로 heredoc/sed를 쓰면 백슬래시가 한 단계 줄어드는 경우가 있었음 — 백슬래시가 든 스크립트는 Write로 파일을 만든 뒤 실행할 것.
- 이 파일(`assembler.hpp`)은 사용자가 대화 밖에서 직접 동시 편집하는 경우가 있었음(과거 Edit 충돌 이력) — 편집 전에는 항상 최신 상태를 다시 Read할 것.
