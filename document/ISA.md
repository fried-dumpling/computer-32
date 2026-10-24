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

## 2. 레지스터

| id | 이름 | 분류 | ABI / 별칭 |
|---|---|---|---|
| 00 | `gen[0]` | GEN | return (반환값) |
| 01 | `gen[1]` | GEN | arg (인자), ret2 (두 번째 반환값) |
| 02–04 | `gen[2]`–`gen[4]` | GEN | arg (인자) |
| 05–0C | `gen[5]`–`gen[12]` | GEN | callee-saved |
| 0D–14 | `gen[13]`–`gen[20]` | GEN | caller-saved |
| 15 | `gen[21]` | GEN | at0 |
| 16 | `gen[22]` | GEN | at1 |
| 17 | `gen[23]` | GEN | kt |
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
| `at0`, `at1` | 깨짐 (사용자 코드는 쓰지 않음) |
| `kt` | 사용자 코드는 쓰지 않음 |
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
- red zone은 없습니다. `stack` 아래 메모리는 언제든 덮어써질 수 있습니다.
- `sbp`는 frame pointer입니다. 쓰지 않는 함수는 보존만 하면 됩니다.

#### 특수 용도 레지스터

- **`at0` / `at1`** (`gen[21]`, `gen[22]`): 어셈블러와 링커가 쓰는 임시 레지스터입니다. `at0`은 pseudo-instruction과 표준 macro(32비트 상수, 0x8000 이상 주소의 `ld`/`st`/`call`)가 쓰고, `at1`은 링커 veneer(먼 `call` 교체)가 씁니다. 사용자 코드는 쓰지 않습니다. 명령 하나나 macro 하나가 끝나면 값이 정해지지 않으며, `call`을 지나면 둘 다 깨집니다. 지금은 pseudo-instruction도 veneer도 없어서 예약만 되어 있습니다.
- **`kt`** (`gen[23]`): 커널 임시 레지스터입니다. 사용자 레지스터를 저장하기 전에 커널이 덮어쓸 수 있는 유일한 레지스터입니다. 다음 경우에 씁니다.
  - ISR 진입 때 스택을 전환하면서 사용자 `stack` 값을 잠시 보관할 때
  - `stack`이 망가진 fault나 panic을 처리할 때
  - 문맥 교환에서 TCB base로 쓸 때
  - 짧은 ISR을 push/pop 없이 처리할 때
  - syscall 핸들러 주소를 읽어 분배할 때

  사용자 코드에서는 늘 쓰레기 값입니다. 커널은 사용자 모드로 돌아가기 전에 `kt`를 0으로 지우고, 인터럽트를 다시 허용하기 전에 `kt`를 다 쓰거나 저장합니다. 하드웨어가 진입 때 PC를 `istack`에 push한다면 1개로 충분합니다. 진입 동작은 아직 정하지 않았습니다.
- **`addr`** (`gen[24]`): 주소 base 전용 레지스터이며 보존됩니다. base는 아무 레지스터나 될 수 있고(`ld`/`st` opcode 하위 5비트), base 하나로는 64KB 창 하나에만 닿습니다. IO(`0x04000000`)나 디스플레이(`0x08000000`) base는 함수 안에서 만들어 쓰면 됩니다(`IO_LDBASE`, 2명령).
- **`zero` / `one` / `full`**: 0, 1, `0xFFFFFFFF`는 레지스터에 담지 말고 이 상수 레지스터를 씁니다.

#### syscall과 인터럽트

- syscall: 번호는 `gen[0]`, 인자는 `gen[1]`–`gen[4]`, 반환은 `gen[0]`(필요하면 `gen[1]`)입니다. 커널은 `gen[0]`, `gen[1]`을 뺀 모든 레지스터와 `flag`를 보존합니다.
- ISR: 모든 레지스터와 `flag`를 보존합니다. `iret`은 PC만 pop하므로, `flag`는 ISR이 직접 저장해야 합니다(`cmp`와 `jmp` 사이에 인터럽트가 들어올 수 있음). pseudo-instruction 중간에 인터럽트가 들어올 수 있으므로 `at0`/`at1`도 저장합니다.

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
| ex-1 | `cpu-id` |
| ex-2 | `panic-vec` |
| ex-3 | `istack` |
| ex-4 | `int-level` |
| ex-5 | `p-base` |
| ex-6 | `p-limit` |
| ex-7 – ex-12 | 예약 |

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
| 77 | `iret` | u.u | | | PC = stack.pop(), 인터럽트 종료 |
| 78 | `intervec` | base | m (0~255) | d (0~63) | int_vec[d] = base + m |
| 79 | `interpri` | u.u | m (0~15) | d (0~63) | int_pri[d] = m |
| 7A | `ldexp` | dest | u.u | id (EXT) | dest = ext[id] |
| 7B | `stexp` | src | u.u | id (EXT) | ext[id] = src |
| 7C | `pctl` | u.u | | en/dis | 페이징 켜기/끄기 |
| 7D | `getabs` | dest | src | u.u | dest = src의 절대(물리) 주소 |
| 7E | `cache` | base | m (부호 있는 8비트) | id | cache[base + m] flush 또는 inval |
| 7F | `atom` | u.u | | len | 뒤따르는 len개 명령어를 원자적으로 실행 |
| 80–FF | `nop` | u.u | | | ISA 확장용 예약 |

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

2026-10-17에 어셈블러를 시트에 맞춤: `pctl` 7C + byte 3 en(1)/dis(0), `getabs` 7D, `flush`/`inval`은 `cache` 7E 하나(id의 md 비트 0x80: flush=1, inval=0), 캐시 id `all` 0x40, `atom` 7F 추가(`atom len`), `intervec` byte 1은 base 레지스터(`intervec R, m, d`).

2026-10-18 어셈블러에 확장 레지스터 `p-limit`(id 5, `plimit`)과 `p-base` 별칭 `pbase`를 추가하고, 캐시 id를 `L1I | L1D`처럼 비트 조합으로 받게 했습니다.

2026-10-18 시트에서 `md` 비트가 flush(1) / inval(0)으로 확정되어 어셈블러와 같아졌습니다. `rol` / `ror`도 시트에 이름대로(*rollleft* / *rollright*) 적혀 있습니다.

남은 차이:

| 항목 | 시트 | 어셈블러 |
|---|---|---|
| 이름 | `jp` / `jpi` / `break` | `jmp` / `ijmp` / `brk` (기존 소스 호환을 위해 유지) |
| 확장 레지스터 이름 | `cpu-id` / `panic-vec` / `int-level` / `p-base` / `p-limit` | `cpuid` / `panicvec` / `intlevel` / `pagebase`(`pbase`) / `plimit`(`pagelimit`). `-`는 빼기 연산자라 이름에 못 씀. id는 ex-n → n-1(0~5)로 같음 |
| `rol` / `ror` | *rollleft* / *rollright* | opcode만 시트와 같음(0C–0F). 회로와 에뮬레이터에서 아직 확인 안 함 |
| ABI 별칭 | `at0` / `at1` / `kt` / `addr` (`gen[21]`–`gen[24]`) | 없음. `gen[21]`–`gen[24]`로 써야 함 |
| pseudo-instruction, 링커 veneer | `at0` / `at1` 예약 | 없음. 링커는 정해진 필드에 재배치 값만 패치함 |
