# 툴체인 버그 목록 (dasm / dlnk)

`program/floatingpoint/float.txt` 작성 중 발견 (2026-10-10, `tools/bin`의 dasm/dlnk 기준).

> **상태 (2026-10-10): 세 가지 모두 수정됨.** `src/assembler-linker` 소스에 반영됐습니다.
> `tools/bin`의 dasm/dlnk는 재빌드하고 교체해야 반영됩니다. 각 항목 끝의 **수정 내용** 참고.
> 수정된 dasm/dlnk에서는 `float.txt`의 우회 코드가 더 이상 필요 없습니다.

## 1. 인자가 정확히 1개인 매크로를 쓰면 `unterminated directive`

**재현**

```asm
#macro PP 1
    addi #1.32B, 4
#end
.text
    PP(gen[1])
```

```
error at t.txt:   unterminated directive
```

- 인자 0개(`PP()`)와 2개 이상(`PP(gen[1], 4)`)은 정상입니다. **1개일 때만** 실패합니다.
- `#ifndef` 블록 안에서 쓰면 매크로 호출이 그 뒤 입력을 전부 삼켜서, 에러가 `#ifdef/#ifndef without matching #end`로 바뀌어 나옵니다. 원인을 찾기가 더 어렵습니다.

**원인**: `tools/assembler/assembler.hpp:691-698` (`handleReplace`, `State::replace_identifier`)

```cpp
case TokenType::closeparen:
    data->depth--;
    if (!data->depth) {
        if (data->args.size()) {          // <- 여기
            data->args.push_back(data->curArg);
            data->curArg.clear();
            data->remainingArg--;
        }
    }
```

- 닫는 `)`에서 마지막 인자를 `args`에 넣는 조건이 `args.size() != 0`입니다.
- 인자가 1개면 쉼표가 없으니 이 시점의 `args`는 비어 있습니다. 그래서 인자가 들어가지 않고 `remainingArg`가 1로 남습니다.
- 확장 조건(`!remainingArg && !depth`)이 끝까지 만족되지 않으니, 파일 끝까지 토큰을 계속 인자로 먹습니다.
- 이 조건은 원래 인자 0개 호출 `NAME()`에서 빈 인자를 넣지 않으려고 둔 것으로 보입니다.

**수정 제안**: 인자 0개는 `remainingArg == 0`으로 구분합니다.

```cpp
        if (data->remainingArg > 0) {
```

**우회**: 1인자 매크로를 쓰지 않습니다. `float.txt`는 `FP_PUTC`를 인라인으로 풀었습니다.

**수정 내용**
- 닫는 `)`에서 마지막 인자를 넣는 조건을 바꿨습니다: 쉼표가 있었거나 인자 토큰이 있으면 넣습니다.
- 그 뒤 인자 개수가 `#macro` 선언과 다르면 `wrong number of macro arguments` 에러를 냅니다. 예전처럼 파일 끝까지 먹지 않습니다.
- 인자 0개 매크로는 괄호 없이(`NAME`) 써도 바로 펼쳐집니다. 예전에는 다음 토큰이 인자로 먹혔습니다.
- 인자가 있는 매크로를 괄호 없이 쓰면 `expected ( after macro name` 에러를 냅니다.

## 2. 내용 없는 주석 줄 `;`이 `unexpected token: ;`

**재현**

```asm
;
.text
    nop
```

```
error at t.txt line 1: ;
 unexpected token: ;
```

- `; `(뒤에 공백)나 `; x`는 정상입니다. 줄 끝에 `;` 하나만 있을 때만 실패합니다.
- 에디터가 줄 끝 공백을 지우면 `; `도 `;`로 바뀌어 같은 에러가 납니다.

**원인**: `tools/assembler/assembler.hpp`의 렉서 규칙

```cpp
{ ";", TokenType::semicolon },          // 244행
...
{ ";[^\n]*", TokenType::comment },      // 285행
```

- `;` 하나는 두 규칙 모두에 같은 길이(1)로 매치됩니다.
- 길이가 같을 때 앞에 있는 `semicolon` 토큰이 이기고, 문법에 `semicolon`이 들어갈 자리가 없어서 parse 에러가 납니다.

**수정 제안** (둘 중 하나)

- `comment` 규칙을 `semicolon`보다 우선하게 합니다.
- 문법에서 `semicolon`을 쓰지 않는다면 `semicolon` 토큰을 제거합니다.

렉서 테이블이 `dasm_table.bin`에 캐시되므로, 수정 후에는 테이블을 지우고 다시 만들어야 합니다.

**우회**: 빈 주석 줄을 쓰지 않습니다.

**수정 내용**: 문법에서 `semicolon` 토큰을 쓰는 곳이 없어서 렉서 규칙 `{ ";", semicolon }`을 지웠습니다. 이제 `;`는 항상 주석입니다. 테이블 버전을 올려서(407) 캐시가 자동으로 다시 만들어집니다.

## 3. dlnk에 빈 / 깨진 오브젝트 파일을 주면 segfault

**재현**

```
dasm bad.txt bad.o     # 어셈블 실패 -> 0바이트 bad.o 생성 (SYNTAX.md 1절: 실패해도 출력 파일은 만들어짐)
dlnk out.bin bad.o     # Segmentation fault (exit 139)
```

- dasm이 실패해도 출력 파일을 만들기 때문에, 빌드 스크립트가 dasm 종료 코드를 확인하지 않으면 바로 이 상황이 됩니다.
- 원인 코드는 아직 확인하지 않았습니다. 오브젝트 헤더 크기나 매직 값을 검사하지 않고 읽는 것으로 추정합니다.

**수정 제안**

- dlnk가 오브젝트 크기와 헤더를 검사하고 에러 메시지를 내도록 합니다.
- 또는 dasm이 실패하면 출력 파일을 만들지 않도록 합니다.

**원인**: `Format::loadRaw`(`common/format.hpp`)가 헤더의 구역 오프셋을 검사 없이 사용했습니다. 문자열을 NUL이 나올 때까지 읽었고, 식은 EOX가 나올 때까지 읽었습니다. 그래서 0바이트 파일이나 깨진 파일이면 버퍼 밖을 읽었습니다.

**수정 내용 (둘 다 적용)**
- `Format::loadRaw`가 `bool`을 반환하도록 바꾸고 다음을 검사합니다. 실패하면 dlnk는 `invalid object file '<경로>'`를 출력하고 종료 코드 -1로 끝납니다.
  - 파일 크기 ≥ 헤더 크기, 저장된 헤더 크기 값
  - 6개 구역이 헤더 바로 뒤부터 순서대로 이어져 파일 끝에서 끝나는지
  - 각 레코드(문자열 NUL, 고정 필드, 식의 EOX)가 자기 구역 안에서 끝나는지
  - 섹션의 start/end가 binary 범위 안인지
- dasm은 실패하면 출력 파일을 쓰지 않고, 이전 실행에서 남은 같은 이름의 파일도 지웁니다.
- 0바이트 파일, 무작위 바이트, 끝이 잘린 오브젝트, 섹션 범위를 망가뜨린 오브젝트로 확인했고, 모두 에러로 끝나며 segfault가 없습니다.
