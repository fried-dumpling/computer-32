# io — ISA-32 키보드 / 터미널 라이브러리

`add_terminal.txt`, `prime_terminal.txt`의 입출력 코드를 함수로 정리한 라이브러리입니다. 문법은 `../floatingpoint/SYNTAX.md`, 명령 의미와 ABI는 루트 `ISA.md`를 따릅니다.

## 파일

| 파일 | 역할 |
|---|---|
| `io.hsm` | 정의(API). IO 주소 상수와 `extern` 선언. 사용하는 프로그램에서 include합니다 |
| `io.asm` | 구현. 따로 어셈블해서 `io.o`로 만들고 링크합니다 |
| `io_harness.asm` | 테스트 프로그램. 모든 함수를 한 번씩 부릅니다 |
| `io_test.py` | harness를 빌드하고, 정해진 키 입력으로 실행한 뒤 터미널 출력을 비교합니다. 에뮬레이터는 `../floatingpoint/isa32emu.py`를 씁니다 |

## 사용법

```asm
#include<io/io.hsm>
```

```
dasm main.asm -o main.o
dasm io/io.asm -o io.o
dlnk main.o io.o -o main.bin        ; main.o를 먼저
```

- `io.hsm`에 있는 상수: `IO_BASE`(0x4000000), `IO_KEY_IN`(0), `IO_KEY_V`(4), `IO_KEY_R`(8), `IO_TERM_OUT`(12).
- 기존 프로그램처럼 `gen[24]`에 `IO_BASE`를 넣어 둘 필요는 없습니다. 각 함수가 `gen[13]`에 직접 만듭니다.
- 문자열은 1 word = 1 글자이고 0으로 끝납니다.
  - 예외: `io_printb`/`io_printlnb`는 `.data`의 문자열 리터럴(`msg: 8, "Hi"`)처럼 바이트로 채운 문자열(word 하나에 4글자, 첫 글자가 최상위 바이트)을 받습니다. 주소는 4의 배수여야 하고, 크기를 문자열 길이보다 크게 잡아서 끝에 0이 오게 합니다.
  - `io_getline`의 프롬프트와 버퍼는 word 문자열입니다. 상수 프롬프트는 목록 문법 `12(4), {'>', ' '}`로 만들 수 있습니다.
- 호출 규약(ISA-32 ABI): 인자는 `gen[1..4]`, 반환은 `gen[0]`입니다. `gen[5..12]`는 건드리지 않고, `gen[1..4]`와 `gen[13..20]`은 덮어씁니다.
- 전역 상태가 없어서 재진입할 수 있습니다. `io_pow10` 테이블은 읽기 전용입니다.
- 코드와 데이터는 0x8000 미만에 있어야 합니다(라벨을 부호 있는 16비트 offset으로 씀).

### API

| 함수 | 입력 | 출력 / 동작 |
|---|---|---|
| `io_putc` | `gen[1]` 문자 | 출력 |
| `io_newline` | | `'\n'` 출력 |
| `io_print` | `gen[1]` 문자열 | 출력, `gen[0]` = 길이 |
| `io_println` | `gen[1]` 문자열 | 출력 + `'\n'`, `gen[0]` = 길이(`'\n'` 제외) |
| `io_printb` | `gen[1]` 바이트 문자열 | 출력, `gen[0]` = 길이 |
| `io_printlnb` | `gen[1]` 바이트 문자열 | 출력 + `'\n'`, `gen[0]` = 길이(`'\n'` 제외) |
| `io_printint` | `gen[1]` | 부호 있는 10진수 출력. -2147483648도 처리합니다 |
| `io_printhex` | `gen[1]` | 16진수 8자리(대문자, `0x` 없음) |
| `io_getc` | | 키가 올 때까지 기다림, `gen[0]` = 문자. 에코 없음 |
| `io_trygetc` | | `gen[0]` = 문자, 대기 중인 키가 없으면 0. 에코 없음 |
| `io_getline` | `gen[1]` 버퍼, `gen[2]` 버퍼 크기(word, 1 이상), `gen[3]` 프롬프트 문자열(0이면 없음) | 프롬프트를 출력하고 한 줄 입력, `gen[0]` = 길이 |

`io_getline` 동작:
- 프롬프트를 먼저 출력하고, 입력한 글자를 에코합니다. Enter(`'\n'` 또는 `'\r'`)로 끝나고, 끝에 `'\n'`을 출력합니다.
- 백스페이스(Digital Keyboard는 8, 127도 받음)는 버퍼에서 한 글자를 지웁니다.
  - Digital Terminal은 글자를 지울 수 없습니다. 그래서 `'\n'` + 프롬프트 + 지금까지의 입력을 새 줄에 다시 출력합니다.
  - 그래서 마지막 줄은 항상 실제 버퍼 내용과 같습니다.
  - 줄이 비어 있으면 무시합니다.
- `gen[2]-1`글자를 넘는 입력은 무시하고 Enter를 기다립니다. 예전 `func_getInput`은 버퍼가 차면 바로 끝났는데, 이 부분이 다릅니다.

### 키보드 프로토콜 (기존 프로그램과 같음)

1. `block`
2. `KEY_V`를 읽습니다. 0이면 1번으로 돌아갑니다.
3. `KEY_IN`에서 문자를 읽습니다.
4. `KEY_R`에 쓰면 그 키를 소비합니다.

`block`은 OoO 파이프라인에서 IO 읽기 순서를 맞추려고 기존 코드에 있던 것을 그대로 따른 것입니다.

## 테스트

```
cd program/io
python io_test.py
```

- 확인하는 항목:
  - `printint`/`printhex`: 0, -1, INT_MAX, INT_MIN 등
  - `print`/`println`의 길이
  - `printb`/`printlnb`: 길이 4(다음 word가 0), 5, 3, 0인 바이트 문자열과 길이
  - `getline`: 프롬프트, 백스페이스 다시 출력, 넘치는 입력, 빈 줄에서 백스페이스, `'\r'`
  - `getc`, `trygetc`(키 없음)
- **현재 결과 (2026-10-10)**: 에뮬레이터에서 ALL PASS입니다. Digital 회로에서는 아직 실행하지 않았습니다.

## 예제: 계산기 `../calc/calc.asm`

```
float calc (+ - * / (), q = quit)
> 1.5 * (2 + -0.5) / 4
= 0.5625
> 1 + * 2
      ^ error
```

- 기능:
  - 수식을 한 줄로 입력합니다. `*` `/`가 `+` `-`보다 먼저 계산되고, 같은 우선순위는 왼쪽부터 계산합니다.
  - 괄호, 단항 `-`/`+`, 토큰 사이 공백을 지원합니다. 숫자는 `fp_str2f` 문법을 따르므로 `1e-3`, `inf`, `nan`도 됩니다.
  - 결과는 유효숫자 7자리로 출력합니다(`DIGITS`).
  - 문법 오류가 나면 처음 잘못된 위치 아래에 `^`를 표시합니다.
  - `q`를 입력하면 `halt`하고, 빈 줄은 무시합니다.
- 구조:
  - 재귀 하강 파서입니다(`parse_expr` → `parse_term` → `parse_factor`).
  - 커서는 `gen[12]`, 오류 위치는 `gen[11]`에 둡니다. 두 레지스터 모두 라이브러리가 보존하는 레지스터라 호출 사이에 유지됩니다.
  - 숫자는 `fp_str2f`의 끝 포인터로 다음 위치를 찾습니다.
- 빌드 (`program/`에서):
  ```
  dasm calc/calc.asm -o calc.o
  dasm floatingpoint/float.asm -o float.o
  dasm io/io.asm -o io.o
  dlnk calc.o float.o io.o -o calc/calc.bin
  ```
- 에뮬레이터에서 확인했습니다: 우선순위, 괄호, 단항 `-`, `inf`/`nan`/`-0`, 오류 위치, 백스페이스 다시 출력.
- 실행 예: `python floatingpoint/isa32emu.py calc/calc.bin "1+2*3\nq\n"`
- 수식 한 줄에 수만~수십만 명령이 듭니다. 대부분 `fp_str2f`/`fp_f2str` 비용이라 Digital에서는 느릴 수 있습니다.

## 예제: BBP 파이 계산기 `../picalc/pi.asm`

BBP 공식으로 π의 16진수를 4자리씩 구하면서, 그때마다 ×10으로 확정된 10진수 자리를 바로 출력합니다. 자세한 내용은 `../picalc/CLAUDE.md`.
