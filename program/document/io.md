# io — 키보드 / 터미널 라이브러리

`archive/add_terminal.txt`, `archive/prime_terminal.txt`의 입출력 코드를 함수로 정리한 라이브러리입니다.
공통 규약(헤더/구현 분리, ABI, 문자열, 주소 제약)은 루트 `CLAUDE.md`의 "Programs" 절을 따릅니다.

## 파일 (`program/io/`)

| 파일 | 역할 |
|---|---|
| `io.hsm` | 정의(API). IO 주소 상수, `extern` 선언, 매크로 `IO_LDB`. 사용하는 프로그램에서 include합니다 |
| `io.asm` | 구현. 따로 어셈블해서 `io.o`로 만들고 링크합니다 |
| `io_harness.asm` | 테스트 프로그램. 모든 함수를 한 번씩 부릅니다 |
| `io_test.py` | harness를 빌드하고, 정해진 키 입력으로 에뮬레이터에서 실행한 뒤 터미널 출력을 비교합니다 |

## 빌드 / 사용법

```asm
#include<io/io.hsm>          ; calc/ 안의 파일이면 ../io/io.hsm
```

```
; program/ 에서
dasm main.asm -o main.o
dasm io/io.asm -o io.o
dlnk main.o io.o -o main.bin        ; main.o를 먼저
```

- `io.hsm`의 상수: `IO_BASE`(0x4000000), `IO_KEY_IN`(0), `IO_KEY_V`(4), `IO_KEY_R`(8), `IO_TERM_OUT`(12).
- 예전 프로그램처럼 `gen[24]`에 `IO_BASE`를 넣어 둘 필요가 없습니다. 각 함수가 `gen[13]`에 직접 만듭니다.
- 문자열은 바이트 문자열입니다. `.data`의 리터럴(`msg: 8, "Hi"`)을 그대로 넘기면 되고, 크기를 길이보다 크게 잡아 끝에 0이 오게 합니다. 정렬은 필요 없습니다. `io_getline`의 프롬프트와 버퍼도 바이트 문자열입니다(`s_prompt: 4, "> "`).
  - 예전의 word 문자열과 `io_printb`/`io_printlnb`는 없앴습니다(2026-10-17). `io_print`/`io_println`이 대신합니다.
- `IO_LDB(dst, base, off)`: 바이트 하나를 0으로 확장해서 읽습니다(`set dst.32B, 0` + `ld.base dst.8L, off`).
- 바이트 쓰기는 `st.base reg.8L, off`입니다. 현재 회로는 이것이 4바이트를 씁니다(루트 `CLAUDE.md`의 알려진 회로 문제). `io_getline`은 앞에서부터 차례로 쓰고 끝에 0을 쓰므로, 버퍼 뒤에 3바이트 여유가 있으면 결과는 같습니다.
- 전역 상태가 없어서 재진입할 수 있습니다. `io_pow10` 테이블은 읽기 전용입니다.

## API

| 함수 | 입력 | 출력 / 동작 |
|---|---|---|
| `io_putc` | `gen[1]` 문자 | 출력 |
| `io_newline` | | `'\n'` 출력 |
| `io_print` | `gen[1]` 문자열 | 출력, `gen[0]` = 길이 |
| `io_println` | `gen[1]` 문자열 | 출력 + `'\n'`, `gen[0]` = 길이(`'\n'` 제외) |
| `io_printint` | `gen[1]` | 부호 있는 10진수 출력. -2147483648도 처리합니다 |
| `io_printhex` | `gen[1]` | 16진수 8자리(대문자, `0x` 없음) |
| `io_getc` | | 키가 올 때까지 기다림, `gen[0]` = 문자. 에코 없음 |
| `io_trygetc` | | `gen[0]` = 문자, 대기 중인 키가 없으면 0. 에코 없음 |
| `io_getline` | `gen[1]` 버퍼, `gen[2]` 버퍼 크기(바이트, 1 이상), `gen[3]` 프롬프트 문자열(0이면 없음) | 프롬프트를 출력하고 한 줄 입력, `gen[0]` = 길이 |

`io_getline` 동작:
- 프롬프트를 먼저 출력하고, 입력한 글자를 에코합니다. Enter(`'\n'` 또는 `'\r'`)로 끝나고, 끝에 `'\n'`을 출력합니다.
- 백스페이스(8 또는 127)는 버퍼에서 한 글자를 지웁니다. Digital Terminal은 글자를 지울 수 없어서 `'\n'` + 프롬프트 + 지금까지의 입력을 새 줄에 다시 출력합니다. 그래서 마지막 줄은 항상 실제 버퍼 내용과 같습니다. 줄이 비어 있으면 무시합니다.
- `gen[2]-1`글자를 넘는 입력은 무시하고 Enter를 기다립니다(예전 `func_getInput`은 버퍼가 차면 바로 끝났음).

### 키보드 프로토콜 (기존 프로그램과 같음)

1. `block`
2. `KEY_V`를 읽습니다. 0이면 1번으로 돌아갑니다.
3. `KEY_IN`에서 문자를 읽습니다.
4. `KEY_R`에 쓰면 그 키를 소비합니다.

`block`은 OoO 파이프라인에서 IO 읽기 순서를 맞추려고 기존 코드를 그대로 따른 것입니다.

## 테스트

```
cd program/io
python io_test.py
```

- `printint`/`printhex`: 0, -1, INT_MAX, INT_MIN 등
- `print`/`println`: 길이 4, 5, 3, 0인 문자열과 반환 길이. 크기를 홀수로 잡아 4바이트 정렬이 아닌 주소도 씁니다
- `getline`: 프롬프트, 백스페이스 다시 출력, 넘치는 입력, 빈 줄에서 백스페이스, `'\r'`
- `getc`, `trygetc`(키 없음)
- **현재 결과 (2026-10-17)**: 에뮬레이터에서 ALL PASS. 라이브러리 단독으로는 Digital 회로에서 돌리지 않았고, 이 라이브러리를 쓰는 calc가 dsim에서 동작했습니다(`calc.md`).

## 이 라이브러리를 쓰는 프로그램

- `calc.md` — 부동소수점 계산기
- `picalc.md` — BBP 파이 계산기
