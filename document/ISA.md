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
- **user**: `add` ~ `wait` (00–74), `syscall` (76)
- **kernel**: `halt` (75), `iret` 이후 (77–FF)
- 2026-11-16 시트에서 `syscall`이 user가 되었습니다. user가 kernel로 들어가는 유일한 진입로이기 때문입니다. opcode는 그대로 76이라 kernel 구간(75–FF) 가운데에 user 명령이 하나 끼어 있습니다.
- `cache`와 `atom`을 user로 옮길지는 보류입니다(2.2절 "미정").

## 2. 레지스터

| id | 이름 | 분류 | ABI / 별칭 |
|---|---|---|---|
| 00 | `gen[0]` | GEN | return, `lret` (반환값, 하위 word) |
| 01 | `gen[1]` | GEN | arg (인자), `hret` (반환값 상위 word) |
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

- 별칭은 시트의 ABI 이름이고 어셈블러는 아직 `gen[n]`만 받습니다(6절). 이름이 붙은 별칭은 `lret`, `hret`, `at`, `kt0`, `kt1`, `addr`입니다(`sbp`는 원래 레지스터 이름).

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
- 분기는 바로 앞 `cmp`/`test`의 flag만 쓰는 것으로 짭니다. 그래서 `subi`로 카운터를 줄여도 0인지 판단하려면 `cmpi`를 넣습니다. 루프 끝이 상수면 `cmpi cnt, N`, 상수가 아니면 끝값 레지스터와 `cmp`를 씁니다. 회로에서는 모든 ALU 명령이 flag를 쓰고 `zero`/`neg`/`pos`도 결과대로 맞습니다(4절, 2026-11-27 확인). 기존 코드는 이 규칙을 지키므로 바꾸지 않았습니다.

### 확장 레지스터 (EXT, `ldexp`/`stexp` 전용)

| id | 이름 |
|---|---|
| ex-0 | `cpu-id` |
| ex-1 | `istack` |
| ex-2 | `int-level` |
| ex-3 | `int-tmplv` |
| ex-4 | `p-base` |
| ex-5 | `p-limit` |
| ex-6 – ex-11 | 예약 |

- 시트가 2026-10-25에 ex-0부터로 번호를 바꿨습니다. 어셈블러 id(0~5)와 같습니다.
- 2026-11-16 시트에서 `panic-vec`을 없애고 `int-tmplv`(ex-3)를 추가했습니다. `istack`은 ex-1, `int-level`은 ex-2로 옮겨졌습니다.
- `int-level`은 `lev`(bit 3:0), `mask`(bit 4), `ker`(bit 7)입니다(2026-11-16 시트). bit 6:5는 저장되지 않습니다(회로 확인: `stexp` 0x5698 → `ldexp` 0x98). `ker`는 이름으로 보아 mode 비트(1 = kernel)입니다. 시트에 따로 설명은 없습니다.
- `int-tmplv`는 같은 형식의 사본입니다. 진입할 때 `int-tmplv = int-level`, `iret`에서 `int-level = int-tmplv`입니다(2.2절).

## 2.2 인터럽트 프로토콜

2026-10-25 결정. `todo.txt`의 "interrupt interface"와 `todo.md`의 "인터럽트 처리 흐름"을 정리한 것입니다.

> **2026-11-16 시트 변경을 일부만 반영함.** 복귀 level과 mode는 IPU 내부 스택이 아니라 `int-tmplv`로 저장합니다(진입: `int-tmplv = int-level`, `iret`: `int-level = int-tmplv`). `int-level`에 `ker`(bit 7)가 생겼고 `panic-vec`은 없어졌습니다. 아래 "복귀 level과 mode", 진입, `iret`, "미정"은 이에 맞춰 고쳤습니다. 중첩할 때 `int-tmplv` 저장 순서와 ISR 예제는 아직 옛 설계 기준입니다(`todo.md` "ISA 시트 동기화").

### 기본 구조

- user와 kernel은 일반 레지스터(`stack` 포함)를 함께 씁니다. 다른 것은 권한(mode)뿐입니다. 모든 인터럽트 핸들러는 kernel에서 돕니다.
- `istack`은 인터럽트가 값을 push하는 임시 스택입니다. kernel 스택과 다르며, 문맥 교환의 임시 스택으로도 씁니다.
- `syscall`도 인터럽트와 같은 경로로 처리합니다.
- 커널이 사용자 레지스터를 저장하기 전에 쓸 수 있는 레지스터는 `kt0`/`kt1`뿐입니다. 핵심 규칙입니다.

### `int-level`과 마스킹

- `int-level` = `{ker (bit 7), mask (bit 4), level (bit 3:0)}`, level은 16단계입니다. 마스킹에는 하위 5비트 `{mask, level}`만 씁니다.
- 수락 조건: `{0, 요청 level} > {mask, 현재 level}`. 비교 하나로 마스킹이 됩니다.
  - mask = 1이면 현재 값이 16 이상이므로 모든 요청이 막힙니다.
  - mask = 0이면 현재보다 높은 레벨만 들어옵니다. 같은 레벨끼리는 중첩되지 않습니다.
  - level 0은 일반 실행 레벨입니다. 수락될 수 있는 인터럽트 레벨은 1–15입니다.
- 진입하면 하드웨어가 mask = 1로 만듭니다. 그래서 ISR이 `kt0`에 `stack`을 저장하기 전에는 중첩되지 않습니다. 중첩을 허용하려면 ISR이 `stexp stack, istack`을 한 뒤 mask를 0으로 내립니다.

### 복귀 level과 mode는 `int-tmplv`에 저장 (2026-11-16 시트)

- 진입하면 하드웨어가 `int-tmplv = int-level`로 복사한 뒤 `int-level`을 바꿉니다. 이전 mode는 복사된 `ker` 비트에 들어 있습니다.
- `iret`은 `int-level = int-tmplv`로 되돌립니다. level, mask, mode가 한 번에 복원됩니다.
- `int-tmplv`는 한 칸뿐입니다. 중첩을 허용하는 ISR은 mask를 내리기 전에 `ldexp`로 `int-tmplv`를 읽어 저장하고, `iret` 전에 `stexp`로 되돌려야 합니다. 정확한 순서는 미정입니다.
- 전에는 IPU 내부 16칸 스택에 `{level, mode}`를 push/pop하는 설계였습니다(2026-10-25). 시트 변경으로 없어졌습니다.
- CPU도 현재 mode 사본을 갖습니다. 디코드 단계의 권한 검사에 씁니다.

### 고정 id (2026-11-18)

- `syscall`은 id 1입니다. `syscall`이 `INT_req`를 올리는 동안 CPU가 `INT_id-_bus` = 1을 냅니다. IPU는 그 칸의 `int_vec`/`int_pri`를 읽어 벡터와 level을 정합니다.
  - 그래서 커널은 `syscall`을 쓰기 전에 `interst R, 0, vec | 1`, `interst zero.32B, L, pri | 1`(L ≥ 1)을 해 둬야 합니다.
- id 0은 panic이나 그와 비슷한 비상용으로 남겨 둡니다(`panic-vec` 대신).
- **level 0 칸으로 요청하면 id 0으로 진입합니다** (2026-11-19 결정). 예를 들어 `int_pri[1]` = 0인 채로 `syscall`을 하면 다음과 같이 됩니다.
  - IPU는 그 칸을 쓰지 않고 level 31과 `int_vec[0]`을 냅니다.
  - CPU는 `int-level` = 0x9f(ker 1, mask 1, lev 0xf)로 `int_vec[0]`에 진입합니다.
  - 복귀 PC는 보통 `syscall`처럼 `syscall` 다음 word입니다. `iret`하면 그 자리로 돌아갑니다.
  - 그래서 커널은 `int_vec[0]`을 반드시 설정해야 합니다. 설정하지 않으면 리셋값 0, 즉 주소 0으로 갑니다.
  - 테스트는 `int_sys0` 21a/21b입니다.
- id 0으로 들어가는 그 밖의 경로(mask = 1일 때의 동기 예외 등)는 미정입니다(아래 "미정").
- 외부 IRQ 선이 어떤 id를 쓰는지(테스트는 id 3)와 atomic abort 등 다른 동기 예외의 id는 미정입니다.

### 권한 검사 (회로 상태, 2026-11-18)

- issue 디코드가 `ISS_kernel`을 냅니다. 1인 것은 75(halt, 74 wait는 0), 77, 78/79, 7A/7B, 7C입니다. 76(`syscall`)은 0입니다.
- 7D(`getabs`), 7E(`flush`/`inval`), 7F(`atom`)는 1절의 "77–FF kernel"과 달리 kernel로 표시되지 않습니다. `cache`/`atom`을 user로 옮길지는 보류 중입니다(아래 "미정").
- 검사는 아직 배선되지 않았습니다. `ISS_kernel`을 받는 곳이 없고, 검사 게이트(`And@-500,2320` = `¬mode ∧ DP_ker`)의 `DP_ker`는 PullDown만 있으며, 결과는 `privilege-fault` LED로만 갑니다. 그래서 user에서 kernel 명령을 실행해도 trap이 나지 않습니다.

### 진입 (하드웨어, commit 경계)

```
int-tmplv = int-level; level = 요청 level; mask = 1; ker = 1
CPU: istack -= 4; mem[istack] = 복귀 PC; PC = int_vec[n]
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
int-level = int-tmplv        ; level, mask, mode(ker) 복원
```

- 권한을 바꾸므로 commit에 도달할 때까지 기다립니다. 추측 실행하지 않습니다.
- 새 PC에서 fetch를 다시 시작합니다. 동작은 오분기 flush와 같습니다.
- (옛 설계) IPU에 pop 요청을 보내고 응답(req/ack)까지 fetch를 멈추는 방식이었습니다. `int-tmplv`로 바뀌면서 IPU 응답을 기다릴 필요가 있는지 다시 봐야 합니다.

### 미정

- atomic abort 등 동기 예외용 고정 id 대역. `syscall`은 id 1, id 0은 비상용으로 정해짐("고정 id"). `syscall`의 level은 `int_pri[1]`로 커널이 정함. `syscall`의 user 분류는 2026-11-16 시트로 정해짐(1절)
- `cache`와 `atom`을 user로 옮길지(보류)
- 동기 예외(fault, `syscall`)는 미룰 수 없습니다. 그래서 mask와 상관없이 처리하는 규칙이 필요합니다. 전에는 mask = 1인 상태에서 생기면 `panic-vec`(double fault)으로 가자는 제안이었는데, 시트에서 `panic-vec`이 없어져 다른 규칙이 필요합니다. id 0을 이 용도로 남겨 두었습니다(2026-11-18). level 0 칸 요청이 id 0(level 31)으로 진입하는 것은 정해졌습니다(2026-11-19, "고정 id"). mask = 1인 상태의 동기 예외가 id 0으로 어떻게 진입하는지(mask와 상관없이 강제 진입하는지)는 남았습니다.
- `int-prev`(이전 mode/level, 읽기 전용 EXT)는 `int-tmplv`가 같은 역할을 하므로 필요 없어 보입니다. syscall 핸들러는 `int-tmplv`의 `ker`로 호출한 쪽이 user였는지 알 수 있습니다.
- 문맥 교환: `int-tmplv`를 `stexp`로 쓸 수 있으므로, 돌아갈 태스크의 mode와 level을 바꿀 수 있습니다. 전에는 IPU 스택 때문에 user 태스크끼리만 바꿀 수 있었습니다.

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
| 08–1F | `gen[0..23]` | in, out |

2026-11-16 시트에서 bit 08–1F가 범용 flag `gen[0..23]` 하나로 합쳐졌습니다. 전에는 `gen1[0..5]`(08–0D)와 `gen2[0..15]`(0E–1D)로 나뉘어 있었습니다.

분기 명령의 조건 필드는 3비트라서 bit 00–07만 조건으로 쓸 수 있습니다.

### ALU 명령이 쓰는 flag (2026-11-27, dsim, `program/test/carry.asm`)

ALU 명령(`add`–`testi`, op 00–19)은 모두 bit 00–05를 씁니다. 명령마다 의미가 있는 bit는 아래와 같고, 나머지 bit에는 adder(A + B)의 carry 등이 부산물로 들어갑니다. 에뮬레이터는 부산물 bit를 정의되지 않은 값으로 보고, 거기에 분기하면 에러를 냅니다. `mov`, `set`, `ld`/`st`, `push`/`pop`, 분기, `call`/`ret`은 flag를 바꾸지 않습니다.

| 명령 | 의미 있는 bit | 내용 |
|---|---|---|
| `add`, `addi`, `addc` | 00–05 | `carry` = bit 31의 carry out. `overflow` = 부호 있는 overflow. `carry4` = bit 3의 carry out(a + b + carry in) |
| `sub`, `subi`, `subc`, `cmp`, `cmpi` | 00–05 | `carry` = **borrow**(부호 없는 a < b(+ borrow in)이면 1). `overflow` = 부호 있는 overflow. `carry4`는 a + ~b + (1 − borrow in)의 bit 3 carry out을 **그대로** 씁니다(반전 안 함: `1 - 1` → 1, `0 - 1` → 0) |
| `shiftl`, `shiftli`, `shiftlc`, `shiftr`, `shiftri`, `shiftrc` | 00–03 | `carry` = 밀려 나간 bit 전체의 OR. shift 양이 0이면 0 |
| `rol`, `roli`, `ror`, `rori` | 00–03 | `carry` = 반대쪽으로 넘어간 bit 전체의 OR. 회전량(mod 32)이 0이면 0 |
| `bxr`, `bor`, `bnd`(+`i`), `test`, `testi` | 00–02 | |

- carry in을 쓰는 명령: `addc` = a + b + carry, `subc` = a − b − carry(carry = borrow), `shiftlc`/`shiftrc` = 비는 bit를 **모두** carry로 채움. 1 bit shift면 보통의 다중 word shift가 되고, 4 bit면 `0x10 shiftlc 4`(carry 1) → `0x10F`입니다.
- 다중 word: 64 bit 덧셈은 `add lo` → `addc hi`, 뺄셈은 `sub lo` → `subc hi`, 1 bit shift는 `shiftl lo` → `shiftlc hi`, `shiftr hi` → `shiftrc lo`입니다. 사이에 `mov`/`set`/`ld`/`st`/`push`/`pop`이 있어도 carry가 남습니다.
- carry를 직접 정하려면 `set R, 0` 뒤에 `cmpi R, v`(v = 0이나 1)를 씁니다. 0 − 1은 borrow라서 carry = 1입니다.
- `pop flag` 바로 뒤의 ALU 명령(사이에 `nop`만 있어도)은 pop한 carry를 보지 못하고, 그 명령이 쓴 flag도 pop한 값에 덮입니다(2026-11-27 dsim). 실제 명령이 하나 이상 끼면 맞습니다.

## 5. 명령어

### ALU (user)

| op | 이름 | byte 1 | byte 2 | byte 3 | 동작 |
|---|---|---|---|---|---|
| 00 | `add` | dest | src | u.u | dest += src |
| 01 | `addc` | dest | src | u.u | dest += src + flag[carry] |
| 02 | `addi` | dest | imm (0~65535) | | dest += imm |
| 03 | `sub` | dest | src | u.u | dest -= src |
| 04 | `subc` | dest | src | u.u | dest -= src + flag[carry] (carry = borrow; 시트의 `src - flag[carry]`는 틀림) |
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
| 11 | `shiftlc` | dest | src | u.u | dest <<= src, 비는 bit는 flag[carry] |
| 12 | `shiftli` | dest | imm | | dest <<= imm |
| 13 | `shiftr` | dest | src | u.u | dest >>= src |
| 14 | `shiftrc` | dest | src | u.u | dest >>= src, 비는 bit는 flag[carry] |
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
| 76 | `syscall` | u.u | | | 시스템 콜 트랩. **user 명령** (2026-11-16, 1절) |
| 77 | `iret` | u.u | | | PC = mem[istack], istack += 4. IPU가 level과 mode를 복원(2.2절) |
| 78 | `interld` | dest | u.u | md\|id | md=0: dest = int_vec[id], md=1: dest = int_pri[id] |
| 79 | `interst` | base | imm (0~255) | md\|id | md=0: int_vec[id] = base + imm, md=1: int_pri[id] = base + imm |
| 7A | `ldexp` | dest | u.u | id (EXT) | dest = ext[id] |
| 7B | `stexp` | src | u.u | id (EXT) | ext[id] = src |
| 7C | `pctl` | u.u | | en/dis | 페이징 켜기/끄기 |
| 7D | `getabs` | dest | src | u.u | dest = src의 절대(물리) 주소 |
| 7E | `cache` | base | m (부호 있는 8비트) | id | cache[base + m] flush 또는 inval |
| 7F | `atom` | u.u | | len | 뒤따르는 len개 명령어를 원자적으로 실행 |
| 80–FF | `nop` | u.u | | | ISA 확장용 예약 |

### `interst` / `interld` (2026-10-26)

인터럽트 테이블(`int_vec`/`int_pri`)에 쓰고(79) 읽습니다(78). 2026-11-14에 시트에서 두 opcode를 맞바꿨습니다(전에는 78 = `interst`, 79 = `interld`). `ldexp`(7A)/`stexp`(7B)처럼 짝수가 읽기, 홀수가 쓰기입니다.

byte 3:

| bit | 7 | 6 | 5–0 |
|---|---|---|---|
| 필드 | md | 예약 (0) | id (0~63) |

- md: `vec` = 0, `pri` = 1
- id는 6비트(64개)입니다.
- 시트 G67의 `md-id = md<<3 | id`는 오타입니다(2026-11-18 확인). 맞는 값은 G65/G66대로 md가 bit 7, id가 bit 5:0입니다. 시트 수정이 남아 있습니다.
- 두 모드 모두 값은 `R[base] + imm`입니다. 데이터패스를 공유하고, 레지스터로 우선순위를 동적으로 정할 수 있습니다. 상수 우선순위는 `interst zero.32B, m, pri | id`로 씁니다.
- `int_pri`는 4비트(0~15)입니다.
  - 2.2절 수락 조건상 **pri = 0인 id는 수락되지 않습니다**. 그래서 id별 disable에 별도 모드가 필요 없습니다.
  - `R[base] + imm`이 15를 넘을 때 하위 4비트를 쓸지, trap으로 처리할지는 미정입니다.
- 두 명령 모두 kernel 전용이라 user에서 실행하면 trap입니다(회로는 아직 trap하지 않음, 2.2절 "권한 검사").
- 실행 시점:
  - `interst`는 retire 시점에 실행합니다. 예외와 인터럽트를 retire에서 인식하는 방침과 같습니다.
  - `interld`는 앞선 `interst`보다 먼저 추측 실행하면 안 됩니다. 레지스터가 아닌 대상에 대한 RAW라서, `ldexp`/`stexp`처럼 직렬화합니다.
- `int_vec`/`int_pri`는 level과 mask처럼 IPU에 둡니다. 그래서 두 명령은 IPU로 가는 요청입니다.
- 어셈블러 문법: `interst R, m, vec | id`, `interld R, pri | id`. `cache`의 `L1I | L1D`처럼 `|`를 쓰고, md를 먼저 씁니다(`SYNTAX.md`). pseudo-mnemonic은 없습니다.
- 고정 id: `syscall` = 1, 0 = panic/비상용(2026-11-18, 2.2절 "고정 id"). atomic abort 등은 미정입니다.

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

2026-11-14 ~ 16 어셈블러를 시트에 맞춤(`interrupt` 브랜치, parser 버전 417, `bin/dasm.exe` 교체됨): `interst` 79 / `interld` 78(`interst R, m, vec|pri | id`, `interld R, vec|pri | id`, byte 3 = md(b7)|id(b5:0)). 확장 레지스터 id `cpuid` 0, `istack` 1, `intlevel` 2, `inttmplv` 3, `pagebase` 4, `plimit` 5. `panicvec`은 없어져 오류입니다.

남은 차이:

| 항목 | 시트 | 어셈블러 |
|---|---|---|
| 이름 | `jp` / `jpi` / `break` | `jmp` / `ijmp` / `brk` (기존 소스 호환을 위해 유지). 시트 이름을 별칭으로 추가할지 미정(라벨 충돌 위험) |
| 확장 레지스터 이름 | `cpu-id` / `istack` / `int-level` / `int-tmplv` / `p-base` / `p-limit` | `cpuid` / `istack` / `intlevel` / `inttmplv` / `pagebase`(`pbase`) / `plimit`(`pagelimit`). `-`는 빼기 연산자라 이름에 못 씀. id는 ex-n = n(0~5)으로 같음 |
| `rol` / `ror` | *rollleft* / *rollright* | 확인함(2026-11-05, dsim `program/test/rotate.asm`): 보통의 32비트 회전, 회전량은 mod 32(32 = 0, 36 = 4). 에뮬레이터도 같게 추가 |
| ABI 별칭 | `lret` / `hret` (`gen[0]`/`gen[1]`), `at` / `kt0` / `kt1` / `addr` (`gen[21]`–`gen[24]`) | 없음. `gen[n]`으로 써야 함. 추가하려면 lexer 키워드라 cache version을 올리고, 기존 프로그램의 라벨·`#define`과 겹치는지 확인 필요 |
| pseudo-instruction, 링커 veneer | `at` 예약 | 없음. 링커는 정해진 필드에 재배치 값만 패치함 |
| `iret` | `istack` 기준 (2.2절) | 인코딩만 있음(77). 에뮬레이터는 인터럽트 명령을 지원하지 않음 |
