# ISA-32 명령어 집합 요약

원본: `ISA-32.xlsx` ("Dumpling Industry's CPU-32 instruction set"). 이 문서는 그 시트를 읽기 쉽게 옮긴 요약입니다. **내용이 다르면 xlsx가 기준입니다.**
어셈블리 문법(라벨, 식, 전처리기 등)은 `src/assembler-linker/SYNTAX.md`에 있습니다.
마지막 절에 현재 어셈블러(`dasm`)의 구현과 시트가 다른 부분을 따로 정리했습니다.

## 1. 명령어 형식

모든 명령어는 4바이트입니다.

| byte 0 | byte 1 | byte 2 | byte 3 |
|---|---|---|---|
| opcode | 피연산자 1 | 피연산자 2 | 피연산자 3 |

- 16비트 즉시값(`imm`, `off`)은 byte 2–3을 함께 씁니다.
- `u.u`는 사용하지 않는 필드입니다.
- 레지스터 피연산자는 1바이트로, `레지스터 id << 3 | 모드`입니다(2·3절).

### 권한 수준
- **user**: `add` ~ `wait` (00–74)
- **kernel**: `halt` 이후 (75–FF)

## 2. 레지스터

| id | 이름 | 분류 | ABI |
|---|---|---|---|
| 00 | `gen[0]` | GEN | return (반환값) |
| 01–04 | `gen[1]`–`gen[4]` | GEN | arg (인자) |
| 05–0C | `gen[5]`–`gen[12]` | GEN | callee-saved |
| 0D–14 | `gen[13]`–`gen[20]` | GEN | caller-saved |
| 15–18 | `gen[21]`–`gen[24]` | GEN | addr |
| 19 | `sbp` | GEN | sbp |
| 1A | `zero` | CONST | 상수 0 |
| 1B | `one` | CONST | 상수 1 |
| 1C | `full` | CONST | 상수 0xFFFFFFFF |
| 1D | `pc` | CPU | special |
| 1E | `stack` | GEN, CPU | special |
| 1F | `flag` | GEN, CPU | special |

### 확장 레지스터 (EXT, `ldexp`/`stexp` 전용)

| id | 이름 |
|---|---|
| ex-1 | `cpu-id` |
| ex-2 | `panic-vec` |
| ex-3 | `istack` |
| ex-4 | `int-level` |
| ex-5 | `p-base` |
| ex-6 – ex-9 | 예약 (extra) |

## 3. 레지스터 모드

| id | 모드 | id | 모드 |
|---|---|---|---|
| 0 | `32` (전체 32비트) | 4 | `S16H` |
| 1 | `16L` | 5 | `S16L` |
| 2 | `8L` | 6 | `S8L` |
| 3 | `8H` | 7 | `S8H` |

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
| 08 | `pc.ov` | out |
| 09 | `sc.ov` | out |
| 0A–0F | `gen1[0..5]` | in, out |
| 10–1F | `gen2[0..15]` | in, out |

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
| 0C | `rol` | dest | src | u.u | dest = dest *rollright* src ※ |
| 0D | `roli` | dest | imm | | dest = dest *rollright* imm ※ |
| 0E | `ror` | dest | src | u.u | dest = dest *rollleft* src ※ |
| 0F | `rori` | dest | imm | | dest = dest *rollleft* imm ※ |
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

※ 시트에는 `rol`이 *rollright*, `ror`가 *rollleft*로 적혀 있습니다. 이름과 반대이므로 확인이 필요합니다.

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
| 8 | 0x80 | md: flush / inval 선택 |

## 6. 현재 어셈블러(`dasm`)와 다른 점

xlsx(2026-09-25 수정본) 기준으로 `src/assembler-linker/assembler/assembler.hpp`와 비교한 결과입니다. 어셈블러의 0x7C 이후 opcode는 시트보다 먼저, 대화로 받은 스펙으로 구현된 것입니다.

| 항목 | 시트 | 어셈블러 |
|---|---|---|
| `pctl` | opcode 7C 하나, en/dis는 byte 3 | `disable` = 7C, `enable` = **7D** (opcode에 더함) |
| `getabs` | 7D | **7E** |
| 캐시 명령 | `cache` 7E 하나, flush/inval은 id의 md 비트 | `flush` = **7F**, `inval` = **80**으로 분리 |
| 캐시 id `all` | 0x40 (bit 7), 0x80은 md | `ALL` = **0x80** |
| `cache`의 m | 부호 있는 8비트 | `imm8` (& 0xFF, 부호 구분 없음) |
| `atom` (7F) | 있음 | 없음 |
| `intervec`의 byte 1 | base 레지스터 | 키워드 `reg` 자체라서 항상 0 |
| 이름 | `jp` / `jpi` / `break` | `jmp` / `ijmp` / `brk` |
| `rol` / `ror` | 이름과 반대 방향으로 적힘 | opcode만 시트와 같음(0C–0F) |

시트 쪽이 최신 의도라면 어셈블러의 `instructionBase`, `ctlBase`, `cacheBase`와 `pctl`/`flush`/`inval`/`intervec` 문법을 시트에 맞춰야 합니다.
