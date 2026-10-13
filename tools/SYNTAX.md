# ISA-32 어셈블리 문법 (dasm / dlnk)

`dasm`(어셈블러)과 `dlnk`(링커)가 받아들이는 소스 문법과 출력 형식을 정리한 문서입니다. `hexdump` 사용법도 1절에 같이 적습니다.
명령어의 **동작 의미**는 CPU 쪽 ISA 문서를 기준으로 하고, 여기서는 어셈블러가 받아들이는 형태와 인코딩만 다룹니다.

## 1. 도구 사용법

```
dasm <input.txt> [-o <output.o>] [-big|-little] [-file] [-token] [-preproc] [-eval] [-bin]
dlnk <input1.o> [input2.o ...] [-o <output.bin>] [-big|-little] [-padbss] [-v]
hexdump <file> [-o <output.txt>] [-big|-little] [-nodec] [-width N] [-bundle N]
```

| 도구 | 옵션 | 동작 |
|---|---|---|
| 공통 | `-o <파일>` | 출력 파일. 기본값: dasm은 입력 파일 확장자를 `.o`로 바꾼 이름, dlnk는 `a.out`, hexdump는 stdout |
| 공통 | `-h`, `-help` | 사용법 출력 |
| 공통 | `-little`, `-big` | 바이트 순서. 기본은 `-little`, 둘을 같이 주면 오류 (아래 참고) |
| dasm | `-file` | 입력 파일 원문 |
| dasm | `-token` | 렉서 토큰 목록 |
| dasm | `-preproc` | 전처리 후 토큰 목록 |
| dasm | `-eval` | parse / evaluate 성공 여부 |
| dasm | `-bin` | 섹션별 바이트 + 최종 오브젝트 바이트 |
| dlnk | `-padbss` | `.bss` 크기만큼 0을 바이너리 끝에 채움 (9절) |
| dlnk | `-v` | 링크 요약(`linked N file(s) -> ...`)을 stderr에 출력 |
| hexdump | `-nodec` | 오른쪽 문자 열 없이 16진수만 |
| hexdump | `-width N` | 한 줄 바이트 수 (기본 4) |
| hexdump | `-bundle N` | 줄 안에서 묶는 바이트 수 (기본: width와 같음) |

- 옵션과 입력의 순서는 자유입니다. 값은 `-o out.o`와 `-o=out.o` 둘 다 됩니다. `--` 뒤의 인자는 전부 입력으로 봅니다.
- 숫자 값은 10진수나 `0x` 16진수입니다.

### 바이트 순서 (`-little` / `-big`)

- dasm: 명령어 워드와 데이터 값을 이 순서로 씁니다. 오브젝트 파일에 순서가 기록됩니다(오브젝트 자체의 헤더·표는 항상 같은 형식).
- dlnk: 모든 입력 오브젝트가 같은 순서여야 합니다. 다르면 `object file 'x.o' is big-endian, linking little-endian (use -big)`로 실패합니다. 재배치도 이 순서로 채웁니다.
- hexdump: 파일 바이트는 그대로 두고 표시만 바꿉니다. `-little`(기본)은 각 묶음(`-bundle`) 안의 바이트를 거꾸로 보여서 리틀엔디안 워드가 값 그대로 읽히고, `-big`은 파일 순서 그대로 보입니다. 오른쪽 문자 열은 항상 파일 순서입니다. 파일 끝에서 모자란 바이트는 리틀엔디안이면 묶음의 왼쪽(상위)이 빕니다.
- 아래 예시의 바이트는 `-big` 기준으로 적었습니다. `-little`이면 각 값의 바이트가 정확히 거꾸로 놓입니다(예: `4, 0x11223344` → `44 33 22 11`, 명령어 `60 D0 00 04` → `04 00 D0 60`).
- stdout에는 요청한 출력(덤프)만 나가고, 오류·경고는 stderr로 나갑니다. 성공하면 기본적으로 아무것도 출력하지 않습니다.
- 종료 코드: 성공 `0`, 처리 실패(입력을 못 읽음, 어셈블/링크 오류) `1`, 명령줄 오류(모르는 옵션, 값 누락, 입력 없음) `2`.
- `dasm`이 실패하면 출력 파일을 만들지 않고, 같은 이름의 기존 파일도 지웁니다.
- 입력 경로는 실행한 셸의 현재 디렉터리 기준입니다.
- 출력 오브젝트 파일 이름(디렉터리·확장자 제외)이 라벨 이름 앞에 붙어 파일 간 충돌을 막습니다(`main.o`의 `loop` → `main#loop`). **링크할 오브젝트들의 파일 이름은 서로 달라야 합니다.**
- `dasm`은 처음 실행할 때 실행 파일 옆에 `dasm_table.bin`(파서 테이블 캐시)을 만듭니다. 실행 파일을 교체하면 이 파일을 지워야 합니다.

## 2. 파일 구조

```asm
#define IO_BASE 0x4000000      ; 전처리 지시어는 어디든 올 수 있음

.data                          ; 첫 섹션 이전에는 지시어 외에 아무것도 올 수 없음
value:  4, 42
buffer: 4*20, 0

.bss
stack_area: 256
stack_end:

.text
start:
    set gen[1].32B, value
    jmp.one zero.32B, start
```

- 소스는 섹션의 나열입니다: `.text`, `.data`, `.bss`. 순서와 반복은 자유입니다.
- 섹션 이름을 붙일 수 있습니다: `.text boot`, `.data tables`. 이름이 없으면 `""`입니다.
- 줄바꿈과 공백은 의미가 없습니다. `lp: nop`처럼 한 줄에 써도 됩니다.
- 주석: `;`부터 줄 끝까지(`;` 하나만 있는 줄도 주석), 또는 `/* ... */`. 주석 안에는 한글(UTF-8)을 써도 됩니다. 주석 밖에서는 ASCII만 허용됩니다.
- 섹션 밖(처음·섹션 사이·끝)에는 `global 이름` / `extern 이름` 선언을 쓸 수 있습니다(8절 끝).

### 섹션별로 쓸 수 있는 문장

| 섹션 | 라벨 `name:` | 명령어 | 데이터 `size, value` / 목록 / 문자열 | 예약 `size` | `name equ expr` |
|---|---|---|---|---|---|
| `.text` | O | O | X | X | **X** |
| `.data` | O | X | O | X | O |
| `.bss` | O | X | X | O | O |

## 3. 라벨 · equ · `$`

```asm
loop:                       ; 라벨 = 현재 섹션 위치
SIZE equ 4*20               ; 이름 = 식의 값 (상수든 라벨 기반이든)
PTR  equ buffer + 8
    jmp.one zero.32B, $     ; $ = 이 명령어(또는 데이터)의 시작 주소
    addi gen[1].32B, $ - loop
```

- 라벨은 앞에서 정의하든 뒤에서 정의하든 참조할 수 있습니다(전방 참조 가능).
- 라벨 값은 섹션 시작 기준의 상대 위치입니다. 최종 절대 주소는 링커가 채웁니다.
- `equ`는 식을 그대로 복사합니다. 뒤에서 정의되는 라벨을 참조하는 `equ`도 됩니다. 단, 그 `equ`를 정의보다 **앞에서** 쓰면 실패합니다.
- `$`는 식 안 어디서든 쓸 수 있습니다. `$ - label`처럼 상수로 끝나면 그대로 계산되고, `jmp $`처럼 주소가 필요하면 링커가 채웁니다.

## 4. 데이터

```asm
.data
v:   4, 0x11223344      ; 4바이트: 11 22 33 44 (-big) / 44 33 22 11 (-little)
h:   2, 0xAABB          ; 2바이트: AA BB (값의 하위 바이트)
buf: 4*20, 0            ; 80바이트를 0으로
p:   4, v               ; 4바이트: v의 최종 주소 (링커가 채움)
     4, 7               ; 라벨 없이도 가능

.bss
area: 256               ; 256바이트 예약 (바이트는 출력되지 않음)
```

- `.data`의 형식은 `size, value`입니다.
  - `size`는 바이트 수이고, **상수여야 합니다**. 뒤에서 정의되는 라벨은 쓸 수 없습니다.
  - `value`는 아무 식이나 됩니다.
- 채우기 규칙:
  - `size <= 4`: 값의 하위 `size`바이트를 넣습니다(바이트 순서는 1절).
  - `size > 4`: 값을 반복하는데, `-big`이면 **끝 기준**으로 맞춥니다. 마지막 4바이트가 값 그대로이고, 남는 앞부분에는 값의 하위 바이트가 들어갑니다. 예: `5, 0x11223344` → `44 11 22 33 44`. `-little`은 이것을 통째로 뒤집은 `44 33 22 11 44`입니다.
- `value`가 라벨처럼 링커가 채워야 하는 값일 때:
  - `size < 4`이면 하위 `size*8`비트만 채워집니다.
  - `size > 4`이면 값 그대로인 4바이트(`-big`은 마지막, `-little`은 처음 4바이트)만 채워지고 나머지는 0입니다.
- `size`가 4의 배수가 아니면 그 뒤 라벨들이 4바이트 정렬에서 벗어납니다.
- `.bss`의 형식은 `size`입니다. 주소 공간만 차지하고 최종 바이너리에는 들어가지 않습니다.

### 목록과 문자열

```asm
.data
arr: 8(2), {1, 0x203, -1}   ; 8바이트, 원소당 2바이트: 00 01 02 03 FF FF 00 00
tbl: 12(4), {arr, tbl}      ; 원소마다 링커가 주소를 채움, 남은 원소 1개는 0
msg: 8, "Hi\n\0"            ; 48 69 0A 00 00 00 00 00
```

- 목록: `total(element), {값, 값, ...}`
  - `total`은 전체 바이트 수, `element`는 원소 하나의 바이트 수입니다. 둘 다 상수여야 하고, `total`은 `element`의 배수여야 합니다.
  - 원소는 각각 `element, 값` 데이터 하나와 똑같이 채워집니다(위 채우기 규칙과 링커가 채우는 값 규칙 그대로).
  - 원소가 `total / element`개보다 적으면 나머지는 0입니다. 많으면 오류입니다. 빈 목록 `{}`는 전체를 0으로 채웁니다.
- 문자열: `total, "문자열"`
  - 문자열의 바이트를 그대로 넣고, `total`까지 남는 부분은 0으로 채웁니다. 끝에 0을 자동으로 붙이지 않으니, 0으로 끝나야 하면 `total`을 문자열보다 크게 잡거나 `\0`을 쓰세요.
  - 문자열이 `total`보다 길면 오류입니다.
  - 이스케이프: `\n`, `\t`, `\r`, `\\`, `\"`, `\'`, `\` 뒤 10진 숫자 최대 3자리(`\0`, `\65`). 그 밖의 `\x`는 `x` 자신입니다.

## 5. 식(expression)

모든 연산은 32비트 부호 있는 정수(`s32`)로 합니다.

| 우선순위 (높음 → 낮음) | 연산자 | 비고 |
|---|---|---|
| 1 | `( )` | |
| 2 | 단항 `~` `!` `+` `-` | |
| 3 | `**` | 거듭제곱, 오른쪽 결합 (`2**3**2` = 512) |
| 4 | `*` `/` `%` | 0 쪽으로 버림 (`-7/2` = -3, `-7%2` = -1) |
| 5 | `+` `-` | |
| 6 | `<<` `>>` | `>>`는 산술 시프트 (`-7>>1` = -4) |
| 7 | `<` `>` `<=` `>=` | 결과 1/0 |
| 8 | `==` `!=` | |
| 9 | `&` | |
| 10 | `^` | |
| 11 | `\|` | |
| 12 | `&&` | |
| 13 | `\|\|` | |
| 14 | `c ? a : b` | |

### 리터럴

| 형식 | 예 |
|---|---|
| 10진 | `42` |
| 16진 | `0x2A` |
| 8진 | `0o52` |
| 2진 | `0b101010` |
| 문자 | `'A'`, `'\n'`, `'\t'`, `'\\'`, `'\65'` (`\` 뒤 숫자는 **10진** 코드) |

문자열 리터럴(`"..."`)은 `.data`의 문자열 데이터(4절)에서만 쓸 수 있습니다. 식 안에서는 쓸 수 없습니다.

### 라벨이 들어간 식

- 라벨이 들어간 식도 연산자 제한이 없습니다(`label * 2`, `a ? b : c` 등). 어셈블 시점에 값을 정할 수 없으면 식 전체가 오브젝트에 저장되고, 링커가 최종 주소로 계산해 채웁니다.
- 명령어의 즉시값 필드는 16비트(`imm16`) 또는 8비트(`imm8`)입니다. 넘치는 상위 비트는 잘립니다.

## 6. 레지스터

레지스터 피연산자는 `<레지스터>.<모드>` 형식이고, 1바이트(`id << 3 | mode`)로 인코딩됩니다.

| 레지스터 | id | 비고 |
|---|---|---|
| `gen[n]`, `reg[n]` | n | 범용, n = 0..24. `[ ]` 안은 식(상수). **범위 검사가 없어서** 25 이상이면 아래 특수 레지스터와 같은 값이 됩니다. |
| `sbp` | 0x19 | |
| `zero` | 0x1A | |
| `one` | 0x1B | |
| `full` | 0x1C | |
| `pc` | 0x1D | |
| `stack` | 0x1E | |
| `flag` | 0x1F | |

| 모드 | 값 | 모드 | 값 |
|---|---|---|---|
| `32B` | 0 | `S16H` | 4 |
| `16L` | 1 | `S16L` | 5 |
| `8L` | 2 | `S8L` | 6 |
| `8H` | 3 | `S8H` | 7 |

모드 이름은 대소문자를 구분하지 않습니다(`32b`, `s16h`).

## 7. 명령어

명령어는 모두 4바이트 워드이고, 메모리에는 `-little`(기본)이면 `[7:0]`부터, `-big`이면 `[31:24]`부터 놓입니다. `R`은 레지스터 바이트, `I16`은 16비트 즉시값, `I8`은 8비트 즉시값입니다.

| 형식 | 문법 | 인코딩 `[31:24] [23:16] [15:8] [7:0]` |
|---|---|---|
| N | `op` | `op 00 00 00` |
| R | `op R` | `op R 00 00` |
| RR | `op R1, R2` | `op R1 R2 00` |
| RI | `op R, I16` | `op R I16hi I16lo` |
| 메모리 | `ld.<base> R, I16` / `st.<base> R, I16` | `(op+base_id) R I16` |
| 분기 | `jmp.<flag> R, I16` / `ijmp.<flag> R, I16` | `(op+flag) R I16` |

### 명령어 목록

| 명령어 | opcode | 형식 |
|---|---|---|
| `add` `addc` `addi` | 00 01 02 | RR RR RI |
| `sub` `subc` `subi` | 03 04 05 | RR RR RI |
| `bxr` `bxri` | 06 07 | RR RI |
| `bor` `bori` | 08 09 | RR RI |
| `bnd` `bndi` | 0A 0B | RR RI |
| `rol` `roli` `ror` `rori` | 0C 0D 0E 0F | RR RI RR RI |
| `shiftl` `shiftlc` `shiftli` | 10 11 12 | RR RR RI |
| `shiftr` `shiftrc` `shiftri` | 13 14 15 | RR RR RI |
| `cmp` `cmpi` | 16 17 | RR RI |
| `test` `testi` | 18 19 | RR RI |
| `set` `sets` | 1A 1B | RI |
| `mov` | 1C | RR |
| `block` | 1D | N |
| `pop` `push` | 1E 1F | R |
| `ld.<base>` | 20 + base_id | 메모리 |
| `st.<base>` | 40 + base_id | 메모리 |
| `jmp.<flag>` | 60 + flag | 분기 |
| `ijmp.<flag>` | 68 + flag | 분기 |
| `call` | 70 | RI |
| `ret` `nop` `brk` `wait` `halt` | 71 72 73 74 75 | N |
| `syscall` `iret` | 76 77 | N |

- `<base>`에는 레지스터 이름이 오고, 모드는 붙이지 않습니다(`ld.gen[3]`, `st.stack`, `ld.zero`). opcode에 그 레지스터의 id가 더해집니다.
- `<flag>`와 값:

  | flag | 값 | flag | 값 |
  |---|---|---|---|
  | `zero` | 0 | `carry4` | 4 |
  | `neg` | 1 | `overflow` | 5 |
  | `pos` | 2 | `one` | 6 (항상) |
  | `carry` | 3 | `gen` | 7 |

### 시스템 명령어

| 명령어 | 문법 | 인코딩 |
|---|---|---|
| `intervec` | `intervec reg, I8, I8` | `78 00 I8 I8`. 첫 피연산자는 키워드 `reg` 자체이고 0으로 인코딩됩니다. **아직 연결이 덜 된 형식입니다.** |
| `interpri` | `interpri I8, I8` | `79 00 I8 I8` |
| `ldexp` / `stexp` | `ldexp R, <ex>` | `7A/7B R 00 ex` |
| `pctl` | `pctl enable` / `pctl disable` | `7D` / `7C` `00 00 00` |
| `getabs` | `getabs R1, R2` | `7E R1 R2 00` |
| `flush` / `inval` | `flush R, I8, <cache>` | `7F/80 R I8 cache` |

`ldexp`/`stexp`의 `<ex>`와 `flush`/`inval`의 `<cache>` 값:

| `<ex>` | 값 | `<cache>` | 값 |
|---|---|---|---|
| `cpuid` | 0 | `L1I` | 0x01 |
| `panicvec` | 1 | `L1D` | 0x02 |
| `istack` | 2 | `TLB` | 0x04 |
| `intlevel` | 3 | `L2` | 0x08 |
| `pagebase` | 4 | `ALL` | 0x80 |

### 예약어

명령어 이름, 레지스터 이름, 모드, 플래그, `<ex>`/`<cache>` 이름, `enable`/`disable`, `global`/`extern`/`equ`는 라벨이나 `#define` 이름으로 쓸 수 없습니다. 예: `test:`, `pos equ 1`, `ALL:`.

## 8. 전처리기

전처리는 파싱보다 먼저, 토큰 단위로 실행됩니다.

### `#define`

```asm
#define IO_BASE 0x4000000
#define SIZE (4*20)              ; 토큰이 여러 개면 반드시 괄호로 묶기
```

- 이름 뒤 **토큰 1개**, 또는 괄호로 묶은 묶음 1개가 값입니다. `#define X A+1`은 `A`만 값이 되고 `+1`은 본문에 남습니다.
- 정의 이후에 나오는 같은 이름의 식별자를 치환합니다. 값 안의 다른 `#define` 이름도 다시 치환됩니다.

### `#macro`

```asm
#macro ADD2 2                    ; 이름, 인자 개수
    addi #1, #2
    addi #1, #2
#end

#macro HALT 0
##loop:                          ; ## 라벨: 호출할 때마다 고유한 이름으로 바뀜
    jmp.one zero.32B, ##loop
#end

.text
    ADD2(gen[1].32B, 3)
    HALT()
```

- 본문에서 `#1`, `#2`, ...은 인자로 치환됩니다.
- 인자는 `,`로 나눕니다. 괄호 안의 `,`는 나누지 않습니다(`M((a, b))`).
- 인자 개수가 선언과 다르면 `wrong number of macro arguments` 오류입니다.
- 인자 0개 매크로는 `NAME()`과 `NAME` 둘 다 됩니다. 인자가 있는 매크로를 괄호 없이 쓰면 `expected ( after macro name` 오류입니다.
- `##이름`은 호출마다 `이름#번호`로 바뀌어서, 같은 매크로를 여러 번 불러도 라벨이 충돌하지 않습니다.

### `#include`

```asm
#include<lib/io.txt>
```

- 경로는 **dasm에 넘긴 최초 입력 파일의 디렉터리** 기준입니다. 하위 폴더의 파일이 다시 include할 때도 마찬가지입니다. 예를 들어 `lib/a.txt` 안의 `#include<b.txt>`는 `lib/b.txt`가 아니라 `b.txt`를 찾습니다.
- 같은 파일을 두 번 include하면 오류입니다. 순환이나 자기 자신 include도 같고, `a/../b.txt`처럼 표기만 다른 같은 경로도 포함됩니다.
- 각 파일의 줄 번호는 따로 추적되므로, 오류 메시지는 해당 파일과 그 파일 안의 줄을 가리킵니다.

### `#ifdef` / `#ifndef`

```asm
#ifndef IO_INC
#define IO_INC 1
    ...
#end

#ifdef DEBUG
    brk
#end
```

- `#define`이나 `#macro`로 정의된 이름인지 검사합니다. 블록은 `#end`로 닫고, `#else`는 없습니다.
- 중첩할 수 있습니다. 조건이 거짓인 블록 안의 지시어는 실행되지 않습니다.
- 매크로 본문 안에도 쓸 수 있고, 매크로가 **펼쳐지는 시점**에 평가됩니다.

### `global` / `extern`

```asm
global main          ; 섹션 밖(파일 처음, 섹션 사이, 끝)에 한 줄에 이름 하나
extern putc
.text
main:
    nop
```

- 섹션 밖에서만 쓸 수 있고, 한 선언에 이름 하나입니다(`global a, b`는 오류). 선언 위치는 사용 위치보다 뒤여도 됩니다.
- 이름은 기본적으로 파일 안에서만 보입니다. 오브젝트에 `파일명#이름`으로 저장되기 때문입니다(1절).
- `global 이름`: 이 파일에 정의된 라벨이나 `equ`를 **접두사 없이** 내보내서 다른 파일이 찾을 수 있게 합니다. 이 파일에 정의되지 않았으면 `global symbol is not defined in this file` 오류입니다.
- `extern 이름`: 다른 파일에 정의된 이름을 씁니다.
  - 값은 링크할 때 채워지므로 명령어 즉시값이나 `.data` 값에는 쓸 수 있습니다.
  - `.data`/`.bss` 크기처럼 상수가 필요한 곳에는 쓸 수 없습니다(`not constant`).
  - 이 파일에서 정의하면 `extern symbol is defined in this file` 오류입니다.
- 링크할 때 `extern` 이름을 `global`로 내보낸 파일이 없으면 `undefined symbol`, 같은 `global` 이름이 두 파일에 있으면 `duplicate symbol`입니다.

```asm
; lib.txt
global add1
.text
add1:
    addi gen[1].32B, 1
    ret

; main.txt
extern add1
.text
    call zero.32B, add1
```

`dlnk main.o lib.o -o out.bin`

## 9. 링크와 메모리 배치

- 최종 바이너리는 주소 0부터 `TEXT` → `DATA` → `BSS` 순서로 배치됩니다.
- 그룹 안에서는 처음 등장한 (종류, 이름) 순서를 따릅니다. 같은 종류·같은 이름의 섹션은 파일 안에서든 파일 사이에서든 하나로 합쳐지고, 입력 파일 순서대로 이어 붙습니다.
- `.bss`는 주소만 차지하고 출력 바이너리에는 들어가지 않습니다. 바이너리는 text와 data 바이트뿐입니다. `-padbss`를 주면 `.bss` 크기만큼 0이 바이너리 끝에 붙습니다(주소는 그대로).
- 다른 파일의 이름은 `global`/`extern`으로 참조합니다(8절). 그 밖의 이름은 파일마다 따로라서, 여러 파일에 같은 라벨 이름이 있어도 충돌하지 않습니다.

## 10. 오류 메시지

소스 위치가 있는 오류(전처리·parse·evaluate)는 GCC 형식이라 편집기에서 바로 열립니다.

```
<파일 경로>:<파일 안 줄 번호>: error: <오류 내용>: <토큰>
    <그 줄의 소스>
```

도구 자체 오류(명령줄, 파일 입출력, 링크)는 `<도구 이름>: error: <내용>` 형식입니다. 예: `dlnk: error: undefined symbol 'foo' (used in 'b.o')`.

| 단계 | 대표 오류 |
|---|---|
| preprocess | `duplicate include`, `invalid include path`, `macro argument out of range`, `wrong number of macro arguments`, `expected ( after macro name`, `expected identifier after #define`, `#end without matching #ifdef/#ifndef`, `#ifdef/#ifndef without matching #end`, `unterminated directive` |
| parse | `unexpected token: <토큰>` (입력 끝이면 `<EOF>`) |
| evaluate | `missing expression` (정의되지 않은 이름), `negative index` (`gen[-1]`), `not constant` (데이터 크기가 상수가 아님), `invalid size` (음수 크기, 원소 크기가 0 이하이거나 전체 크기를 나누지 못함), `initializer too long` (목록 원소나 문자열이 크기보다 큼), `global symbol is not defined in this file`, `extern symbol is defined in this file` |
| link | `invalid object file '<경로>'`, `undefined symbol '<이름>'`, `duplicate symbol '<이름>'` |
| 공통 | `cannot read file '<경로>'`, `cannot write file '<경로>'`, `no input file`, `unknown flag '<옵션>'`, `missing value for '<옵션>'`, `invalid value for '<옵션>': '<값>'`, `unexpected argument '<인자>'` |
