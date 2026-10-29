# ISA-32 명령어 집합 요약

원본: `ISA-32.xlsx` ("Dumpling Industry's CPU-32 instruction set"). 이 문서는 그 시트를 읽기 쉽게 옮긴 요약입니다. **내용이 다르면 xlsx가 기준입니다.**
어셈블리 문법(라벨, 식, 전처리기 등)은 `document/SYNTAX.md`에 있습니다.
마지막 절에 현재 어셈블러(`dasm`)의 구현과 시트가 다른 부분을 따로 정리했습니다.

## 1. 명령어 형식

모든 명령어는 4바이트입니다.

| byte 0 | byte 1 | byte 2 | byte 3 |
|---|---|---|---|
| opcode | 피연산자 1 | 피연산자 2 | 피연산자 3 |

- 16비트 즉시값(`imm`, `off`)은 byte 2–3을 함께 씁니다.
- 명령어는 32비트 word 하나이고 메모리에는 리틀엔디안으로 들어갑니다. word로 보면 byte 0(opcode)이 bit 31:24, byte 3이 bit 7:0입니다. 그래서 메모리 주소로는 opcode가 +3, byte 3이 +0에 있습니다.
- `u.u`는 사용하지 않는 필드입니다.
- 레지스터 피연산자는 1바이트로, `레지스터 id << 3 | 모드`입니다(2·3절).

### 권한 수준
- **user**: `add` ~ `wait` (00–74)
- **kernel**: `halt` 이후 (75–FF)
- 재분류 검토 중: `syscall`은 user가 kernel로 들어가는 유일한 진입로라서 user에서 실행할 수 있어야 합니다. `cache`와 `atom`을 user로 옮길지는 보류입니다. 정해지면 opcode 배치도 다시 봐야 합니다(2.2절 "미정").

## 2. 레지스터

| id | 이름 | 분류 | ABI / 별칭 |
|---|---|---|---|
| 00 | `gen[0]` | GEN | return (반환값) |
| 01 | `gen[1]` | GEN | arg (인자), ret2 (두 번째 반환값) |
| 02–04 | `gen[2]`–`gen[4]` | GEN | arg (인자) |
| 05–0C | `gen[5]`–`gen[12]` | GEN | callee-saved |
| 0D–14 | `gen[13]`–`gen[20]` | GEN | caller-saved |
| 15 | `gen[21]` | GEN | at |
| 16–17 | `gen[22]`–`gen[23]` | GEN | kt (kt0, kt1) |
| 18 | `gen[24]` | GEN | addr (base) |
| 19 | `sbp` | GEN | sbp |
| 1A | `zero` | CONST | 상수 0 |
| 1B | `one` | CONST | 상수 1 |
| 1C | `full` | CONST | 상수 0xFFFFFFFF |
| 1D | `pc` | CPU | special |
| 1E | `stack` | GEN, CPU | special |
| 1F | `flag` | GEN, CPU | special |

- 별칭은 시트의 ABI 이름이고 어셈블러는 아직 `gen[n]`만 받습니다(6절).

### 호출 규약 (ABI)

시트의 ABI 열을 풀어 쓴 규칙입니다(2026-10-24).

#### 보존

| 레지스터 | 호출 뒤 |
|---|---|
| `gen[0]`–`gen[4]` | 깨짐 |
| `gen[5]`–`gen[12]` | 보존 (callee가 저장) |
| `gen[13]`–`gen[20]` | 깨짐 |
| `at` | 깨짐 (사용자 코드는 쓰지 않음) |
| `kt0`, `kt1` | 사용자 코드는 쓰지 않음 |
| `addr` | 보존 |
| `sbp` | 보존 |
| `stack` | 보존 (호출 전과 같은 값) |
| `flag` | 깨짐 |

#### 인자와 반환값

- 인자 1–4: `gen[1]`–`gen[4]`. 5번째부터는 caller가 마지막 인자부터 push하고, 돌아온 뒤 caller가 `addi stack`으로 정리합니다. `call`이 복귀 주소를 push하므로 callee에서 5번째 인자는 `[stack + 4]`입니다.
- 반환: 4바이트 이하는 `gen[0]`, 8바이트 이하는 `gen[0]`(하위 / 첫 word) + `gen[1]`(상위 / 둘째 word)입니다. 몫과 나머지, 값과 끝 포인터(`fp_str2f`), 값과 오류 코드처럼 두 값을 돌려줄 때 씁니다.
- 8바이트보다 큰 값: caller가 결과 버퍼 주소를 `gen[1]`에 넘기고, 인자는 `gen[2]`부터 씁니다.

#### 스택

- `stack`은 아래로 자라고 4바이트 정렬을 유지합니다. `push`는 `stack -= 4; mem[stack] = v`입니다.
- red zone은 없습니다. ISR은 `istack`을 쓰므로 인터럽트가 사용자 `stack` 아래를 덮어쓰지는 않습니다. 그래도 지금은 `stack` 아래 메모리에 값을 두지 않는 것을 규칙으로 합니다.
- `sbp`는 frame pointer입니다. 쓰지 않는 함수는 보존만 하면 됩니다.

#### 특수 용도 레지스터

- **`at`** (`gen[21]`): 어셈블러와 링커가 쓰는 임시 레지스터입니다.
  - 쓰는 곳: pseudo-instruction, 표준 macro(32비트 상수, 0x8000 이상 주소의 `ld`/`st`/`call`), 링커 veneer(먼 `call`을 대신하는 짧은 코드)
  - veneer도 같은 `at`를 씁니다. `at`는 `call`을 지나면 깨지는 레지스터라 `call` 시점에는 살아 있는 값이 없기 때문입니다.
  - 사용자 코드는 쓰지 않습니다. 명령 하나나 macro 하나가 끝나면 값이 정해지지 않습니다.
  - 임시값이 둘 필요한 pseudo-op(예: 32비트 상수를 먼 주소에 store)은 만들지 않습니다. 그런 경우는 t 레지스터로 직접 풀어 씁니다.
  - 지금은 pseudo-instruction도 veneer도 없어서 예약만 되어 있습니다.
- **`kt0` / `kt1`** (`gen[22]`, `gen[23]`): 커널 임시 레지스터입니다. 사용자 레지스터를 저장하기 전에 커널이 덮어쓸 수 있는 레지스터는 이 둘뿐입니다.
  - `kt0`: ISR 진입 때 이전 `stack`을 보관합니다(2.2절). 프로토콜은 `kt0` 하나로 끝납니다.
  - `kt1`: `kt0`이 `stack`을 들고 있는 동안 쓰는 두 번째 임시 레지스터입니다. 문맥 교환의 TCB base, syscall 핸들러 주소를 읽어 분배할 때, push/pop 없는 짧은 ISR에 씁니다.
  - 사용자 코드에서는 늘 쓰레기 값입니다.
  - 커널은 사용자 모드로 돌아가기 전에 `kt1`을 0으로 지웁니다(커널 값 유출 방지). `kt0`에는 사용자 자신의 `stack`이 들어 있으므로 지우지 않아도 됩니다.
  - 인터럽트를 다시 허용(mask = 0)하기 전에 `kt0`/`kt1`을 다 쓰거나 저장합니다.
- **`addr`** (`gen[24]`): 주소 base 전용 레지스터이며 보존됩니다. base는 아무 레지스터나 될 수 있고(`ld`/`st` opcode 하위 5비트), base 하나로는 64KB 창 하나에만 닿습니다. IO(`0x04000000`)나 디스플레이(`0x08000000`) base는 함수 안에서 만들어 쓰면 됩니다(`IO_LDBASE`, 2명령).
- **`zero` / `one` / `full`**: 0, 1, `0xFFFFFFFF`는 레지스터에 담지 말고 이 상수 레지스터를 씁니다.

#### syscall과 인터럽트

- syscall: 번호는 `gen[0]`, 인자는 `gen[1]`–`gen[4]`, 반환은 `gen[0]`(필요하면 `gen[1]`)입니다. 커널은 `gen[0]`, `gen[1]`을 뺀 모든 레지스터와 `flag`를 보존합니다.
- ISR: 모든 레지스터, `flag`, `at`를 보존합니다. 진입 순서와 복귀 순서는 2.2절입니다.

#### 레지스터 역할 (권장)

직접 어셈블리를 짤 때의 권장 배치입니다. 의무는 아니고, 역할이 다르면 함수 머리 주석에 적습니다(`; s0 = buf, s1 = i`). `s`/`t` 이름은 설명용이며 시트의 별칭이 아닙니다.

- callee-saved는 `gen[5]`부터 연속으로 씁니다. 그래야 k개를 쓰는 함수가 `gen[5]`–`gen[4+k]`만 push/pop하면 됩니다.
- caller-saved와 callee-saved의 같은 칸은 같은 역할입니다(`t2` ↔ `s2`, 번호 차이 8). leaf 함수를 non-leaf로 바꿀 때 번호만 옮기면 됩니다.

| caller-saved | 역할 | callee-saved | 역할 |
|---|---|---|---|
| `gen[13]` t0 | base (IO, 디스플레이, 먼 주소) | `gen[5]` s0 | 주 데이터 포인터 (버퍼, 객체) |
| `gen[14]` t1 | scratch (바이트 load, 중간값) | `gen[6]` s1 | 루프 카운터 / 인덱스 |
| `gen[15]` t2 | 루프 카운터 / 인덱스 | `gen[7]` s2 | 누적값 |
| `gen[16]` t3 | 누적값 / 상태 | `gen[8]` s3 | 루프 끝 / 길이 / end pointer |
| `gen[17]` t4 | src 포인터, 테이블 값 | `gen[9]` s4 | 두 번째 포인터 (dst) |
| `gen[18]` t5 | dst 포인터 / end pointer | `gen[10]` s5 | 두 번째 누적값 / 상위 word |
| `gen[19]` t6 | 상수 / 루프 끝 (마스크, 10) | `gen[11]` s6 | 상수 / base (루프 내내 쓰는 IO base) |
| `gen[20]` t7 | 안쪽 루프 카운터 / 예비 | `gen[12]` s7 | 함수 전체 상태 (calc의 cursor) |

- t 레지스터: leaf 함수, 또는 `call`을 넘지 않는 값에 씁니다. `t0`/`t1` 배치는 io와 fp 라이브러리가 이미 쓰는 방식입니다(`IO_LDBASE(gen[13])`, `IO_LDB(gen[14], ...)`).
- s 레지스터: `call`을 넘어 살아 있어야 하는 값에 씁니다. `call` 뒤에도 쓸 인자는 함수 첫머리에서 s 레지스터로 옮깁니다. 그 외의 인자는 `gen[1]`–`gen[4]`를 그대로 작업 레지스터로 써도 됩니다.
- 결과는 처음부터 `gen[0]`에 누적하면 마지막 `mov`가 필요 없습니다.
- `cmp`/`test`가 아닌 ALU 명령은 flag를 깨뜨립니다. 그래서 `subi`로 카운터를 줄여도 0인지 판단하려면 `cmpi`가 필요합니다. 루프 끝이 상수면 `cmpi cnt, N`, 상수가 아니면 끝값 레지스터와 `cmp`를 씁니다.

### 확장 레지스터 (EXT, `ldexp`/`stexp` 전용)

| id | 이름 |
|---|---|
| ex-0 | `cpu-id` |
| ex-1 | `panic-vec` |
| ex-2 | `istack` |
| ex-3 | `int-level` |
| ex-4 | `p-base` |
| ex-5 | `p-limit` |
| ex-6 – ex-11 | 예약 |

- 시트가 2026-10-25에 ex-0부터로 번호를 바꿨습니다. 어셈블러 id(0~5)와 같습니다.
- `int-level`은 5비트 `{mask, level[3:0]}`입니다(2.2절). 실제 값은 IPU에 있고, `ldexp`/`stexp`는 IPU로 가는 요청입니다.

## 2.2 인터럽트 프로토콜

2026-10-25 결정. `todo.txt`의 "interrupt interface"와 `todo.md`의 "인터럽트 처리 흐름"을 정리한 것입니다.

### 기본 구조

- user와 kernel은 일반 레지스터(`stack` 포함)를 함께 씁니다. 다른 것은 권한(mode)뿐입니다. 모든 인터럽트 핸들러는 kernel에서 돕니다.
- `istack`은 인터럽트가 값을 push하는 임시 스택입니다. kernel 스택과 다르며, 문맥 교환의 임시 스택으로도 씁니다.
- `syscall`도 인터럽트와 같은 경로로 처리합니다.
- 커널이 사용자 레지스터를 저장하기 전에 쓸 수 있는 레지스터는 `kt0`/`kt1`뿐입니다. 핵심 규칙입니다.

### `int-level`과 마스킹

- `int-level` = `{mask (bit 4), level (bit 3:0)}`, 16단계입니다.
- 수락 조건: `{0, 요청 level} > {mask, 현재 level}`. 비교 하나로 마스킹이 됩니다.
  - mask = 1이면 현재 값이 16 이상이므로 모든 요청이 막힙니다.
  - mask = 0이면 현재보다 높은 레벨만 들어옵니다. 같은 레벨끼리는 중첩되지 않습니다.
  - level 0은 일반 실행 레벨입니다. 수락될 수 있는 인터럽트 레벨은 1–15입니다.
- 진입하면 하드웨어가 mask = 1로 만듭니다. 그래서 ISR이 `kt0`에 `stack`을 저장하기 전에는 중첩되지 않습니다. 중첩을 허용하려면 ISR이 `stexp stack, istack`을 한 뒤 mask를 0으로 내립니다.

### 복귀 level과 mode는 IPU가 관리

- IPU가 수락할 때마다 `{이전 level, 이전 mode}`를 IPU 내부 스택에 push합니다.
- 중첩은 레벨이 엄격히 올라갈 때만 일어나므로 깊이는 최대 15입니다. 16칸이면 넘치지 않습니다.
- 인터럽트는 mask = 0일 때만 수락되므로, 복귀할 때 mask는 항상 0으로 돌아갑니다. mask 값은 IPU 스택에 저장하지 않습니다.
- `iret`은 IPU에 pop 요청을 보내고, IPU가 돌려준 `{level, mode}`로 복원합니다.
- CPU도 현재 mode 사본을 갖습니다. 디코드 단계의 권한 검사에 씁니다. 진입하면 kernel이 되고, `iret`하면 IPU 응답의 mode가 됩니다.

### 진입 (하드웨어, commit 경계)

```
IPU: push {level, mode}; level = 요청 level; mask = 1
CPU: istack -= 4; mem[istack] = 복귀 PC; mode = kernel; PC = int_vec[n]
```

- 하드웨어가 메모리에 쓰는 것은 PC 1 word뿐입니다(store queue에 1회 적재).
- 복귀 PC는 외부 인터럽트라면 아직 commit되지 않은 첫 명령, `syscall`이라면 다음 명령입니다.

### ISR 소프트웨어

```asm
    mov   kt0, stack           ; 이전 stack (user 또는 바깥 ISR)
    ldexp stack, istack        ; stack = istack (PC 저장 칸)
    push  kt0
    push  flag
    push  ...                  ; 쓸 레지스터 (at 포함)
    stexp stack, istack        ; 중첩 전 필수: istack을 현재 위치로
    ; (필요하면 mask = 0으로 중첩 허용)
    ; ... 처리 ...
    ; (mask = 1)
    pop   ...
    pop   flag
    pop   kt0
    stexp stack, istack        ; istack = PC 저장 칸
    mov   stack, kt0           ; 이전 stack 복원
    set   kt1.32B, 0           ; user로 돌아갈 때
    iret
```

- `mov`, `ldexp`/`stexp`, `push`/`pop`은 flag를 바꾸지 않습니다. 그래서 첫 `cmp`나 ALU 명령보다 앞이면 `push flag`의 위치는 어디든 됩니다.
- `stexp stack, istack`은 중첩을 허용하기 전에 반드시 합니다. 소프트웨어 push는 `stack`만 줄이고 `istack`은 그대로 둡니다. 그 상태에서 중첩이 일어나면 하드웨어가 `istack - 4`에 PC를 써서 바깥 ISR이 저장한 값을 덮어씁니다.
- 중첩된 ISR도 같은 순서를 씁니다. 그때 `kt0`은 바깥 ISR의 stack이고, `stack`은 `istack`이 되어 결과적으로 아무 영향이 없습니다.

### `iret` (직렬화 명령)

```
PC = mem[istack]; istack += 4
IPU pop → level, mode 복원 (mask = 0)
```

- 권한을 바꾸므로 commit에 도달할 때까지 기다립니다. IPU 요청은 추측 실행으로 보내지 않습니다.
- IPU 응답을 기다리는 동안 fetch를 멈춥니다(req/ack). 응답이 오면 새 PC에서 fetch를 다시 시작합니다. 동작은 오분기 flush와 같습니다.

### 미정

- `syscall`을 user 명령으로 옮기는 것(1절 권한 수준). `syscall`의 level과 벡터 번호(`interst` id 0~63 중 하나). atomic abort 등 동기 예외용 고정 id 대역
- `cache`와 `atom`을 user로 옮길지(보류)
- 동기 예외(fault, `syscall`)는 미룰 수 없습니다. 그래서 mask와 상관없이 처리하는 규칙이 필요합니다. 제안: mask = 1인 상태에서 생기면 `panic-vec`(double fault)
- `int-prev`(IPU 스택 맨 위의 이전 mode/level, 읽기 전용 EXT) 추가 여부. syscall 핸들러가 호출한 쪽이 user였는지 알아야 할 때 필요합니다.
- 문맥 교환은 가장 바깥 레벨에서 user 태스크끼리만 합니다. IPU 스택 맨 위의 mode가 그대로 맞기 때문입니다. kernel 스레드까지 바꾸려면 맨 위 항목을 쓸 수 있어야 합니다.

## 3. 레지스터 모드

| id | 모드 | id | 모드 |
|---|---|---|---|
| 0 | `32` (전체 32비트) | 4 | `S16H` |
| 1 | `16L` | 5 | `S16L` |
| 2 | `8L` | 6 | `S8L` |
| 3 | `8H` | 7 | `S8H` |

- `set`은 모드에 해당하는 비트에만 imm을 쓰고 나머지 비트는 유지합니다. 2026-10-18에 회로에서 `0xFFFFFFFF`에 써서 확인했습니다.

  | 모드 | 쓰는 비트 | 예 (`0xFFFFFFFF`에 `set`) |
  |---|---|---|
  | `32` | 31:0 | `set .32B 0x5A5A` → `00005A5A` |
  | `S16H` | 31:16 | `set .S16H 0x2468` → `2468FFFF` |
  | `16L`, `S16L` | 15:0 | `set .16L 0x1357` → `FFFF1357`, `set .S16L 0x5678` → `FFFF5678` |
  | `8L`, `S8L` | 7:0 | `set .8L 0x12` → `FFFFFF12`, `set .S8L 0x9A` → `FFFFFF9A` |
  | `8H`, `S8H` | 15:8 | `set .8H 0x34` → `FFFF34FF`, `set .S8H 0xBC` → `FFFFBCFF` |

  - `set`에서는 `S16L`/`S8L`/`S8H`가 `16L`/`8L`/`8H`와 같게 동작합니다. 부호 확장은 하지 않습니다.
- `ld`/`st`에서는 레지스터 모드가 메모리 접근 폭입니다(회로의 `D_MD` = 모드 하위 2비트). 메모리는 바이트 주소 + 리틀엔디안이고, 정렬되지 않은 주소도 됩니다. 2026-10-17에 회로에서 확인했습니다.
  - `ld R.32B` = word, `ld R.16L` = R[15:0]에 half, `ld R.8L` = R[7:0]에 byte, `ld R.8H` = R[15:8]에 byte. 부분 load는 나머지 비트를 **유지**합니다(`0xFFFFFFFF`에 `ld .8L` → `0xFFFFFF22`). 0으로 확장하려면 먼저 `set R.32B, 0`을 합니다.
  - `st R.16L` / `st R.8L` / `st R.8H`는 R[15:0] / R[7:0] / R[15:8]을 그 폭(2 / 1 / 1바이트)만큼만 씁니다. 처음에는 store의 `D_MD`가 항상 0이라 4바이트를 썼는데, 회로를 고친 뒤 2026-10-18에 다시 확인했습니다(`0xAAAAAAAA`에 `st .8L 0x34` at +1, `st .8H` at +3 → `12AA34AA`).

## 4. 플래그 (`flag` 레지스터)

| bit | 이름 | 방향 |
|---|---|---|
| 00 | `zero` | out |
| 01 | `neg` | out |
| 02 | `pos` | out |
| 03 | `carry` | in, out |
| 04 | `carry4` | out |
| 05 | `overflow` | out |
| 06 | `one` | const (항상 1) |
| 07 | `gen` | in, out |
| 08–0D | `gen1[0..5]` | in, out |
| 0E–1D | `gen2[0..15]` | in, out |

분기 명령의 조건 필드는 3비트라서 bit 00–07만 조건으로 쓸 수 있습니다.

## 5. 명령어

### ALU (user)

| op | 이름 | byte 1 | byte 2 | byte 3 | 동작 |
|---|---|---|---|---|---|
| 00 | `add` | dest | src | u.u | dest += src |
| 01 | `addc` | dest | src | u.u | dest += src + flag[carry] |
| 02 | `addi` | dest | imm (0~65535) | | dest += imm |
| 03 | `sub` | dest | src | u.u | dest -= src |
| 04 | `subc` | dest | src | u.u | dest -= src - flag[carry] |
| 05 | `subi` | dest | imm | | dest -= imm |
| 06 | `bxr` | dest | src | u.u | dest ^= src |
| 07 | `bxri` | dest | imm | | dest ^= imm |
| 08 | `bor` | dest | src | u.u | dest \|= src |
| 09 | `bori` | dest | imm | | dest \|= imm |
| 0A | `bnd` | dest | src | u.u | dest &= src |
| 0B | `bndi` | dest | imm | | dest &= imm |
| 0C | `rol` | dest | src | u.u | dest = dest *rollleft* src |
| 0D | `roli` | dest | imm | | dest = dest *rollleft* imm |
| 0E | `ror` | dest | src | u.u | dest = dest *rollright* src |
| 0F | `rori` | dest | imm | | dest = dest *rollright* imm |
| 10 | `shiftl` | dest | src | u.u | dest <<= src |
| 11 | `shiftlc` | dest | src | u.u | dest <<= src, flag[carry] |
| 12 | `shiftli` | dest | imm | | dest <<= imm |
| 13 | `shiftr` | dest | src | u.u | dest >>= src |
| 14 | `shiftrc` | dest | src | u.u | dest = flag[carry], dest >> src |
| 15 | `shiftri` | dest | imm | | dest >>= imm |
| 16 | `cmp` | src1 | src2 | u.u | src1 - src2로 플래그 설정 |
| 17 | `cmpi` | src | imm | | src - imm으로 플래그 설정 |
| 18 | `test` | src1 | src2 | u.u | src1 & src2로 플래그 설정 |
| 19 | `testi` | src | imm | | src & imm으로 플래그 설정 |

### 레지스터 · 스택 · 메모리 (user)

| op | 이름 | byte 1 | byte 2 | byte 3 | 동작 |
|---|---|---|---|---|---|
| 1A | `set` | dest | imm (0~65535) | | dest = imm |
| 1B | `sets` | dest | imm | | dest = imm (higher set sign) |
| 1C | `mov` | dest | src | u.u | dest = src |
| 1D | `block` | u.u | | | 파이프라인 차단 |
| 1E | `pop` | dest | u.u | | dest = stack.pop() |
| 1F | `push` | src | u.u | | stack.push(src) |
| 20–3F | `ld`+base | dest | off (-32768~32767) | | dest = RAM[base + off] |
| 40–5F | `st`+base | src | off (-32768~32767) | | RAM[base + off] = src |

`ld`/`st`는 opcode 하위 5비트에 base 레지스터 id(00–1F)를 넣습니다.

### 흐름 제어 (user)

| op | 이름 | byte 1 | byte 2–3 | 동작 |
|---|---|---|---|---|
| 60–67 | `jp`+(f[n]) | base | off (-32768~32767) | 조건이 맞으면 PC = base + off |
| 68–6F | `jpi`+(f[n]) | base | off | PC = base + off. 조건 반전 분기(`jp`의 반대)로 쓰임 |
| 70 | `call` | base | off | stack.push(PC), PC = base + off |
| 71 | `ret` | u.u | | PC = stack.pop() |
| 72 | `nop` | u.u | | 아무것도 안 함 |
| 73 | `break` | u.u | | 디버그 트랩 |
| 74 | `wait` | u.u | | 인터럽트가 올 때까지 대기 |

- `jp`/`jpi`의 opcode 하위 3비트는 조건 플래그 번호입니다(4절의 bit 00–07). 예: `jp`+`zero` = 60, `jp`+`one` = 66(무조건).
- 주소는 PC 상대가 아니라 **base 레지스터 + 오프셋**입니다.

### 커널 명령어

| op | 이름 | byte 1 | byte 2 | byte 3 | 동작 |
|---|---|---|---|---|---|
| 75 | `halt` | u.u | | | CPU 정지 |
| 76 | `syscall` | u.u | | | 시스템 콜 트랩 |
| 77 | `iret` | u.u | | | PC = mem[istack], istack += 4. IPU가 level과 mode를 복원(2.2절) |
| 78 | `interst` | base | imm (0~255) | md\|id | md=0: int_vec[id] = base + imm, md=1: int_pri[id] = base + imm |
| 79 | `interld` | dest | u.u | md\|id | md=0: dest = int_vec[id], md=1: dest = int_pri[id] |
| 7A | `ldexp` | dest | u.u | id (EXT) | dest = ext[id] |
| 7B | `stexp` | src | u.u | id (EXT) | ext[id] = src |
| 7C | `pctl` | u.u | | en/dis | 페이징 켜기/끄기 |
| 7D | `getabs` | dest | src | u.u | dest = src의 절대(물리) 주소 |
| 7E | `cache` | base | m (부호 있는 8비트) | id | cache[base + m] flush 또는 inval |
| 7F | `atom` | u.u | | len | 뒤따르는 len개 명령어를 원자적으로 실행 |
| 80–FF | `nop` | u.u | | | ISA 확장용 예약 |

### `interst` / `interld` (2026-10-26)

인터럽트 테이블(`int_vec`/`int_pri`)에 쓰고(78) 읽습니다(79).

byte 3:

| bit | 7 | 6 | 5–0 |
|---|---|---|---|
| 필드 | md | 예약 (0) | id (0~63) |

- md: `vec` = 0, `pri` = 1
- id는 6비트(64개)입니다.
- 두 모드 모두 값은 `R[base] + imm`입니다. 데이터패스를 공유하고, 레지스터로 우선순위를 동적으로 정할 수 있습니다. 상수 우선순위는 `interst zero.32B, m, pri | id`로 씁니다.
- `int_pri`는 4비트(0~15)입니다.
  - 2.2절 수락 조건상 **pri = 0인 id는 수락되지 않습니다**. 그래서 id별 disable에 별도 모드가 필요 없습니다.
  - `R[base] + imm`이 15를 넘을 때 하위 4비트를 쓸지, trap으로 처리할지는 미정입니다.
- 두 명령 모두 kernel 전용이라 user에서 실행하면 trap입니다.
- 실행 시점:
  - `interst`는 retire 시점에 실행합니다. 예외와 인터럽트를 retire에서 인식하는 방침과 같습니다.
  - `interld`는 앞선 `interst`보다 먼저 추측 실행하면 안 됩니다. 레지스터가 아닌 대상에 대한 RAW라서, `ldexp`/`stexp`처럼 직렬화합니다.
- `int_vec`/`int_pri`는 level과 mask처럼 IPU에 둡니다. 그래서 두 명령은 IPU로 가는 요청입니다.
- 어셈블러 문법: `interst R, m, vec | id`, `interld R, pri | id`. `cache`의 `L1I | L1D`처럼 `|`를 쓰고, md를 먼저 씁니다(`SYNTAX.md`). pseudo-mnemonic은 없습니다.
- 고정 id 배정(`syscall`, atomic abort 등)은 미정입니다(2.2절 "미정").

### `cache` 명령의 id 바이트

| bit (1부터) | 값 | 의미 |
|---|---|---|
| 1 | 0x01 | L1I |
| 2 | 0x02 | L1D |
| 3 | 0x04 | TLB |
| 4 | 0x08 | L2 |
| 5, 6 | 0x10, 0x20 | 미사용 |
| 7 | 0x40 | all: 캐시 전체 |
| 8 | 0x80 | md: flush(1) / inval(0) |

## 6. 현재 어셈블러(`dasm`)와 다른 점

2026-10-17에 어셈블러를 시트에 맞춤: `pctl` 7C + byte 3 en(1)/dis(0), `getabs` 7D, `flush`/`inval`은 `cache` 7E 하나(id의 md 비트 0x80: flush=1, inval=0), 캐시 id `all` 0x40, `atom` 7F 추가(`atom len`).

2026-10-18 어셈블러에 확장 레지스터 `p-limit`(id 5, `plimit`)과 `p-base` 별칭 `pbase`를 추가하고, 캐시 id를 `L1I | L1D`처럼 비트 조합으로 받게 했습니다.

2026-10-18 시트에서 `md` 비트가 flush(1) / inval(0)으로 확정되어 어셈블러와 같아졌습니다. `rol` / `ror`도 시트에 이름대로(*rollleft* / *rollright*) 적혀 있습니다.

남은 차이:

| 항목 | 시트 | 어셈블러 |
|---|---|---|
| 이름 | `jp` / `jpi` / `break` | `jmp` / `ijmp` / `brk` (기존 소스 호환을 위해 유지) |
| 확장 레지스터 이름 | `cpu-id` / `panic-vec` / `int-level` / `p-base` / `p-limit` | `cpuid` / `panicvec` / `intlevel` / `pagebase`(`pbase`) / `plimit`(`pagelimit`). `-`는 빼기 연산자라 이름에 못 씀. id는 ex-n = n(0~5)으로 같음 |
| `rol` / `ror` | *rollleft* / *rollright* | opcode만 시트와 같음(0C–0F). 회로와 에뮬레이터에서 아직 확인 안 함 |
| ABI 별칭 | `at` / `kt0` / `kt1` / `addr` (`gen[21]`–`gen[24]`) | 없음. `gen[21]`–`gen[24]`로 써야 함 |
| pseudo-instruction, 링커 veneer | `at` 예약 | 없음. 링커는 정해진 필드에 재배치 값만 패치함 |
| `iret` | `istack` 기준 (2.2절) | 인코딩만 있음(77). 에뮬레이터는 인터럽트 명령을 지원하지 않음 |
| `interst` / `interld` (2026-10-26) | 78 / 79, byte 3 = md(b7)\|id(b5:0) | `tools` 브랜치에 구현(2026-10-26, 미커밋, parser 버전 416): `interst R, m, vec\|pri \| id`, `interld R, vec\|pri \| id`. `bin/dasm.exe`는 아직 옛 빌드 |
