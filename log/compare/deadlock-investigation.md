# calc 정지 문제 조사 기록 (2026-10-10 ~ 10-11)

> **현재 상태 (2026-10-11)**
> - 원인 1: Memory Manager 회수 버퍼 clear 배선 오류. **확정·수정됨.** memtest4 test 2가 해당합니다.
> - 원인 2(후보): flush의 id clear 벡터가 빔(id-manager epoch 복구 의심). **미확정.** memtest4 test 7이 해당하며, 근거는 "M4-7.csv 분석"에 있습니다.
> - 원인 3: flush와 같은 반클럭에 RS dispatch된 call/ret의 SP ±4가 버려짐(SCS 갱신 mux). **확정·수정됨.** calc 재시작과 memtest8 `3`/`k`가 해당하며, 내용은 "원인 3" 절에 있습니다.
> - 원인 4: RS의 FL/SC 의존 태그가 commit과 같은 반클럭에 삽입된 새 생산자를 잃음(`inst-wfile` mux 우선순위). **확정·수정됨.** calc `1+2` 에러가 해당하며, 내용은 "원인 4" 절에 있습니다.
> - 원인 5: front-end stall 중 flush가 오면 flush 이전 wrong-path 명령이 RS에 삽입됨(DC/ISS valid가 stall 중에도 매 클럭 재계산됨). **확정·수정됨.** calc 곱셈 `inf`가 해당하며, 내용은 "원인 5" 절에 있습니다.
> - 원인 3–5 수정 후 headless 검증(아래 "수정 후 검증"): calc 8개 식이 모두 에뮬레이터와 같고, memtest8 회귀도 전부 통과했습니다. selftest T–X와 GUI 실행 확인, 원인 2(memtest4 test 7) 재확인은 아직 남았습니다.

`program/calc.asm`(float + io 라이브러리 계산기)이 에뮬레이터에서는 정상인데 Digital 회로(`cpu-32.dig`)에서 멈추거나 재시작하는 문제를 추적한 기록입니다. 가설, 실험, 결과를 순서대로 정리했습니다.

- 테스트 프로그램은 `program/test/`에 있습니다(selftest, memtest*). calc는 `program/`에 있습니다.
- 덤프 원본은 `log/raw/`, 분할·비교 파일은 `log/compare/`에 있습니다.
- 빌드: `dasm X.asm X.o` → `dlnk run.txt X.o`
- 에뮬레이터는 `program/floatingpoint/isa32emu.py`입니다. 테스트 프로그램은 모두 에뮬레이터에서 통과하는 것을 먼저 확인했습니다.

## 증상

- calc에서 `5/2`를 입력하면 결과가 나오지 않고, 제목이 다시 출력됩니다. 프로그램이 주소 0으로 다시 점프한 것입니다.
- 입출력(`io_getline`, `io_print`)은 정상이고, 계산 부분에서만 문제가 생깁니다.

## 가설과 실험

| # | 가설 | 실험 | 결과 | 판정 |
|---|---|---|---|---|
| 1 | 32비트 상수 적재 순서 문제: `set .32B lo` 다음 `set .S16H hi`를 하면 S16H가 하위 16비트를 지울 수 있음 | selftest A (`S16H` 단독 동작 확인) | `ABCD1234`. S16H는 하위 16비트를 유지함 | **기각** (안전하게 `S16H` → `16L` 순서로 바꿔 두었음) |
| 2 | calc에서 처음 쓰는 명령 11개(`push`, `pop`, `bndi`, `bori`, `bxr`, `bxri`, `testi`, `ijmp.neg`, `shiftl`/`shiftr` reg, `halt`) 중 하나가 오동작 | `selftest.asm` A–S: 명령별 검사, push/pop, 재귀 24단, 레지스터 8개 save/restore | 전부 통과 | **기각** |
| 3 | 라이브러리 안에서 명령이 조합될 때의 문제 | selftest T–X (fp_add/div/mul/str2f/f2str) | T(fp_add)에서 멈춤 | 라이브러리 경로에서 재현됨 |
| 4 | 라이브러리의 특정 위치 문제 | 체크포인트마다 문자를 출력하는 trace 빌드 | trace를 넣자 T–V가 통과. W(str2f)에서 재시작, 재시작 직후 P에서 스택 포인터가 8 word(`FFFFFFE0`) 어긋남 | 코드는 같고 **타이밍에 따라 결과가 달라짐** → 회로 문제 |
| 5 | store→load 순서/포워딩 문제 | `memtest.asm` a–j (같은 주소, 다른 base, 부분 주소, store 14개, push×8+call, RMW 루프) | a(같은 주소 st→ld, 16회 루프)에서 멈춤 | 루프에서 재현 |
| 6 | 단발 st→ld 자체가 문제 | `memtest2.asm`: 단발 l0, s0, a1–a8 + 루프 L1(block 있음), L2(block 없음) | 단발은 전부 통과, L1 통과, **L2 멈춤** | 루프로 반복할 때만 문제 |
| 7a | (B) 메모리 순서 위반 flush(`fl`)가 load id를 누수시킴 | `memtest3.asm` 1: 분기 없이 unroll한 st→ld 16쌍 | 반복 실행해도 누수 없음 | **기각** |
| 7b | (A) 분기 오예측으로 flush된 wrong-path ld/st가 id를 누수시킴 | `memtest3.asm` 4: 루프에서 st X / ld Y | 누수 없음 | **기각** |
| 7c | 관찰 | `memtest3.asm` 3, 7, 8을 빠르게 반복 | 3, 8은 약 10회, 7은 약 5회 만에 id-manager가 가득 차서 정지 | 커밋되지 않는 명령이 생김 |
| 8 | flush된 wrong-path store가 이후 같은 주소의 load에 보임 | `memtest4.asm` 1–7 | 1, 5, 6 통과 / 2, 3, 4, 7 정지. 첫 키 입력에서 랜덤하게 정지하기도 함. 출력값은 항상 정확 | 데이터는 맞음 → 진행(liveness) 문제 |
| 9 | 분기 근처의 `block`/`nop`이 flush되면 해제되지 않음 | memtest4 결과의 패턴 분석 | M4.csv 덤프로 직접 확인 | **기각** (덤프 결과와 맞지 않음) |
| 10 | 덤프 분석 (`log/M4.csv`, memtest4 `2`를 연속으로 누른 것) | 아래 "M4.csv 분석" 참고 | 정지 명령 특정: `ld gen[1], [0x2D0]` @0x108, id 9 | 확정 |
| 11 | 로드/스토어 주소 비교가 하위 비트만 봄 + 나중 스토어에도 로드가 대기 (0x2D0 vs 0x3D0) | `memtest5.asm`: `ld X ; st X+OFF`, OFF = 4/0/0x100/0x200/0x400/0x1000/0x80/0x40/0x10000 | **전부 통과** | **기각** (스토어 주소가 즉시 준비되는 경우에는 문제 없음) |
| 12 | (사용자 가설) 포워딩이 메모리 응답보다 우선해서 실제 load 데이터가 skip됨 | M4.csv 간접 분석 (포워딩 신호는 덤프에 없음) | 같은 주소 스토어가 없었으므로, 정상적인 포워딩은 불가능. 메모리 응답 예상 시점(addr+6) 바로 전에 나중 push의 주소가 확정됨 | **기각** (원인 1로 설명됨) |
| 13 | 주소가 늦게 확정되는 store(push)가 load 직후에 오는 경우 | `memtest6.asm`: `ld X ; push`(push 대상 X+0x104/0x100/0x200/0x1000/0x40), `ld ; jmp ; call` | 에뮬레이터 통과. 회로 결과는 받지 않음 | **기각** (원인 1로 설명됨) |
| 14 | (사용자 확인) Memory Manager의 회수 버퍼 clear 배선 오류 | 회로 직접 확인 | 아래 "원인 1" 참고 | **확정, 수정됨** (memtest4 test 2) |
| 15 | flush 때 id clear 벡터(`CLR_inst`)가 비어서 wrong-path 명령이 살아남음 | `log/raw/M4-7.csv` (memtest4 test 7) 분석 | 아래 "M4-7.csv 분석" 참고 | **유력, 미확정** |

## M4.csv 분석 (memtest4 `2` 연속 입력)

- 각 행은 반 클럭입니다.
- `R(n)`은 레지스터 id n−1입니다. 예: R2=gen[1], R30=pc, R31=stack.
- `MR-x`는 load의 목적 레지스터 바이트입니다.

### 정지 경과

| row | 사건 |
|---|---|
| 2281 | t2 루프 탈출 분기가 오예측되어 JPU flush. 스토어 버퍼 S2–S4의 `st [slotA]`(id 9, D, 1)는 정상적으로 무효화됨 |
| 2289 | `ld gen[1], [0x2D0]`(@0x108)이 id 9로 L4에 할당됨 |
| 2292 | load 주소 준비(MAV=1). 같은 때 `call puthex`의 push `st [0x3D0]`(id B)가 S4에 할당됨 |
| 2296 | push 주소가 0x3D0으로 확정됨. 그 전까지 SAM4에는 이전 점유자의 값 0x2CC가 남아 있었음 |
| 2298 | load 데이터 예상 시점(다른 load 15개는 모두 addr+6에 데이터 도착) → **오지 않음** |
| 2284 이후 | 커밋 없음. RS와 id가 가득 차서 fetch 정지(H=1) |

결과적으로 가장 오래된 load(id 9)는 데이터를 기다리고, 더 나중 스토어(id B)는 커밋이 끝나야 나갈 수 있습니다. 서로를 기다리는 **데드락**입니다.

### 성공 실행과의 비교 (run1 row 500 ↔ run3 row 2288)

- 직전 상태(스토어 id, load id 9, 슬롯 L4, 타이밍)가 같습니다.
- 차이는 나중에 들어온 스토어입니다.
  - 성공: wrong-path `st [0x2CC]`. 바로 flush되었고, 4행 뒤 load 데이터가 도착함.
  - 실패: 커밋을 기다리는 실제 push `st [0x3D0]`이 계속 유효한 상태로 남아 있음.
- 원인이 아닌 것으로 확인한 것:
  - load의 epoch 비트: epoch 1인 load도 정상인 경우가 있음
  - "주소 미확정인 나중 스토어가 있으면 막힌다": 다른 load 7개는 같은 상황에서 정상
- (당시 추정, 이후 기각) 하위 바이트가 같은 주소(0xD0)인 나중 스토어의 주소가 load 데이터 도착 직전에 확정된 것이 원인이라고 봤습니다. 실제 원인은 "원인 1"입니다.

### 분할 파일

- `log/compare/M4SUC.csv` = M4 rows 240–999, `log/compare/M4ERR.csv` = M4 rows 2028–2787 (각 760행)
  - run1과 run3를 load 기준(오프셋 1788행)으로 맞췄습니다.
  - `step`/`cycle`/`Ecycle`은 0부터 다시 매겼습니다.
- 레지스터, 명령 id의 epoch 비트, stale S1을 제외하면 **data row 265에서 처음 갈라집니다.** 성공 실행에만 있는 flush(`FL_JPU`, `CLR_inst`=ids B, C) 지점입니다.
- ERR의 마지막 커밋은 data row 약 256(M4 row 2284)입니다.

## 원인 1: Memory Manager 회수 버퍼 clear 배선 오류 (확정, 수정됨, 2026-10-11)

memtest4 test 2 정지(위 M4.csv 분석)의 원인입니다. 사용자가 회로에서 확인하고 수정했습니다.

- Memory Manager는 **전송 버퍼**와 **회수 버퍼** 두 단으로 되어 있습니다. 전송 버퍼에 있던 load는 다음 클럭에 회수 버퍼로 옮겨집니다.
- 그래서 flush로 전송 버퍼를 clear하면, 회수 버퍼는 **한 클럭 뒤에** clear되어야 합니다. 그래야 방금 옮겨 간 wrong-path 요청이 지워집니다.
- 실제 회로에서는 회수 버퍼의 clear가 이 지연된 신호가 아니라 **회수 버퍼 자신의 clear**에 잘못 연결되어 있었습니다.
- 이 오류가 JPU flush(`FL_JPU`/`clr`)와 같은 시점에 겹치면, 살아 있어야 하는 load(id 9, `ld [0x2D0]`)의 메모리 응답이 사라졌습니다. 그 결과 load가 데이터를 끝내 받지 못했고, 데드락으로 이어졌습니다.
- 정리하면 가설 11(주소 하위 비트), 12(포워딩 우선), 13(주소가 늦게 정해지는 push)은 원인이 아니었습니다. SUC/ERR을 가른 것도 push 주소가 아니라 **JPU flush와 메모리 요청의 타이밍**이었습니다.
- 덤프에서 본 다음 현상도 이것으로 설명됩니다.
  - 같은 코드인 M4 row 1090의 load는 정상이었습니다. 그때는 앞선 store commit 때문에 메모리 응답이 2행 늦어서, flush와 겹치지 않았습니다.
  - 주소 비교기는 load-buffer와 store-buffer 모두 32비트입니다. 주소 비교는 원래 원인이 아니었습니다.

## M4-7.csv 분석 (memtest4 test 7, 원인 1 수정 후)

`log/raw/M4-7.csv` (7570행)입니다. test 7은 `st slotA=0x66 ; loop{ st slotB } ; ld slotA ; call puthex`입니다.

주소는 다음과 같습니다(빌드 결과가 `program/run.txt`와 같음).
- `t7_lp`: 0x1AC `st slotB` / 0x1B0 `subi` / 0x1B4 `cmpi` / 0x1B8 `ijmp`
- 0x1BC `ld slotA` / 0x1C0 `jmp show`
- `show`: 0xCC `call puthex` / 0xD0 `jmp menu`
- `puthex`: 0x1C4부터. HEXD는 0x1C8 `mov`, 0x1CC `shiftri`, 0x1D0 `bnd`, 0x1D4 `cmpi`, 0x1D8 `jmp.neg` 순서입니다.

### 이번에는 load 문제가 아님

- load 21개가 모두 주소 준비 후 +6 또는 +8에 데이터를 받았습니다.
- 마지막 commit은 row 3002입니다. 그 뒤로 commit이 없고 `halt-FT-DC-ISS=1`로 정지합니다.
- 정지 시점의 `R2`(gen[1])는 0x66으로 맞는 값입니다.

### 정지 경과

id 표기 `1C`는 epoch 1, id C입니다(`inst_ID`).

| row | 사건 |
|---|---|
| 2947–2969 | `cmpi`(1B), 루프 `ijmp`(1C), `ld slotA`(1D), `jmp show`(1E), `call puthex`(1F) 순서로 dispatch. `call`을 fall-through로 예측해 0xD0 → menu PUTC들이 id **00–05**(epoch 0, F→0 wrap)로 들어옴 |
| 2971 | **1번째 JPU flush**: `call`(1F) 판정. `CLR_inst`=[0–5]로 정상 |
| 2973–2983 | puthex 경로가 id **10, 11, 12, 13**으로 할당됨. ← **epoch 1. 정상이면 00–03이어야 함** (아래 비교) |
| 2983 | **2번째 JPU flush**: 루프 `ijmp`(1C)의 탈출 예측이 틀림(루프 계속). 지워야 할 id는 D, E, F, 0, 1, 2, 3인데 **`CLR_inst`가 비어 있음** (두 반클럭 모두 0) |
| 2984 | LSU는 flush를 반영함. L4(`ld slotA`, id D)와 S2(`call` push, id F)가 무효화되고, SP가 3D0→3D4로 복구됨 |
| 2985–3001 | id가 지워지지 않았으므로 wrong-path인 D(`ld`), E(`jmp show`), F(`call`), 0(`set gen18`), 1(`mov`), 2(`shiftri`)가 **차례로 commit됨** |
| 2985–2993 | flush 뒤에도 front-end의 puthex 명령 `bnd`(13), `cmpi`(1D), `jmp.neg`(1E)가 dispatch됨 |
| 2995– | 그다음에야 올바른 경로 `st slotB`(1F), `subi`(00), `cmpi`(01), `ijmp`(02)가 들어옴 |
| 3001– | commit head가 id 3(wrong-path `bnd`)에서 멈춤 |

정지 원인은 다음과 같습니다.
- `bnd`가 기다리는 생산자 tag(id 2 등)가 이미 commit·재할당되었습니다. 그래서 값이 영영 오지 않습니다.
- `jmp.neg`(1E, dep 00D)는 `cmpi`(1D)를 기다리고, 그 `cmpi`는 다시 `bnd`를 기다리는 연쇄입니다.
- 그 결과 RS와 id가 막혀 fetch가 정지합니다.

### 비교: flush의 `CLR_inst`가 비는 경우는 이것 하나뿐

M4.csv와 M4-7.csv에 있는 JPU flush 약 85개를 모두 확인했습니다.
- `CLR_inst`가 빈 경우는 **2983 하나뿐**입니다.
- id가 F→0으로 넘어가는 범위를 지우는 flush(wrapCLR)는 30개 이상이며, 모두 정상이었습니다. 그러므로 wrap 자체는 원인이 아닙니다.

branch id가 F인 flush에서 재할당된 id의 epoch은 다음과 같습니다.

| row | branch | 재할당 id | 기대 |
|---|---|---|---|
| M4-7 201 | 0F (jmp) | 10 | 10 ✓ |
| M4-7 1003 | 0F | 10 | 10 ✓ |
| M4-7 2413 | 1F (jmp.neg) | 00 | 00 ✓ |
| **M4-7 2971** | **1F (call)** | **10** | **00 ✗** |

2971 이후 puthex 명령은 id 0–3이 epoch 1로 할당되었습니다. 같은 epoch 1인 C–F보다 **나이가 많은 것처럼** 보이게 된 것입니다. id-manager가 clear 범위를 "clr_id부터 할당 포인터까지, wrap 여부는 epoch 비교"로 계산한다면, 2983의 범위는 다음과 같이 됩니다.
- clr=1C이고 할당 쪽 epoch도 1이므로 wrap이 아니라고 판단합니다.
- 범위는 C < id < 4가 되어 비게 됩니다.
- 이것은 덤프에서 본 `CLR_inst`=0과 맞습니다.

### 가설과 남은 의문

- **유력 가설**: 2971의 flush(branch id F, `call`)에서 할당 포인터를 0으로 되돌릴 때 epoch을 잘못 복구했습니다. 그래서 다음 flush의 clear 벡터가 비었습니다.
- **미확정**인 부분은 두 가지입니다.
  - 같은 1F→00 복구인데 2413(`jmp.neg`)은 정상입니다. `call`/JPU 경로의 clr_ep, 또는 flush와 같은 클럭에 있던 dispatch(2971에 `58A0000C/04` 진입)와 관련 있을 수 있습니다.
  - flush 뒤에도 front-end 명령(bnd/cmpi/jmp.neg)이 들어온 것이 epoch 문제의 결과인지, 별개의 문제인지 모릅니다.

## 현재 결론

- 소프트웨어(float/io/calc)는 에뮬레이터에서 전부 통과합니다. 문제는 회로입니다.
- 원인 1(Memory Manager 회수 버퍼 clear)은 수정되었습니다. memtest4 test 2가 그 경우입니다.
- 남은 문제(test 7)는 load/store 경로가 아니라 **flush의 id clear 범위 계산**(id-manager의 epoch)으로 보입니다.

## 다음 단계

1. 다음 덤프에 추가할 프로브:
   - id-manager: `clr_id`, `clr_ep`, `epoch`(할당 epoch 레지스터), 할당 포인터, `clear_16`/`clr_16` 내부 벡터
   - flush-mux: JPU가 보내는 flush id/epoch (특히 `call` 판정일 때)
   - front-end(fetch/decode) kill 신호
2. memtest4 test 7을 다시 실행해 2971/2983과 같은 패턴(branch id F인 `call` flush 뒤 다른 flush)에서 확인합니다.
3. 회로가 고쳐질 때까지 calc는 회로에서 안정적으로 동작하지 않습니다.

## calc 재시작 문제 (2026-10-11, memtest8)

### 증상

- calc에서 `5/2`나 `1+2`를 입력하면 결과 대신 제목이 다시 출력됩니다. 주소 0으로 점프해 재시작한 것입니다.
- 에뮬레이터에서는 정상입니다.

### RAM 덤프 (`log/ram/calc-dump.txt`)

| 항목 | 결과 |
|---|---|
| 코드 영역 | 바이너리와 같음 (덮어써지지 않음) |
| 스택 쓰기 | 정상 (`0x7B1 = 0x1C`, `0x7B0 = 0x11F4`) |
| 남아 있던 스택 프레임 (0x7A0–0x7AF) | 에뮬레이터 `pc = 0xCCC` 시점과 일치 |
| `fp_bigB` | `[1, 0…]`: `fp_big_set32(B,1)`까지 실행, `fp_bigdiv`는 실행 안 됨 |

- 호출 체인: `eval` → `parse_expr` → `parse_factor` → `fp_str2f`(0x1DC) → `fp_big_mul10n`(복귀 0xCCC).
- 재시작 구간 (`fp_str2f` 안):

```
call fp_big_set32 → (call fp_big_zero, st ×14, ret) → st ×2 → ret
cmpi / jmp.neg / set / mov
call fp_big_mul10n (K=0: mov, cmpi, jmp.zero, ret)
jmp.one → call fp_bigdiv   ← 여기까지 오지 못함
```

- `5/2`와 `1+2` 모두 첫 숫자를 읽는 같은 지점에서 죽었습니다.
- 설명하지 못한 값: `0x7A4 = 0x1588`, `0x7A7 = 1`.

### 가설과 실험

memtest8의 각 테스트를 연속 6번 눌러 실행했습니다. 같은 테스트도 실행마다 결과가 달라질 수 있기 때문입니다.

| # | 가설 | 실험 | 결과 | 판정 |
|---|---|---|---|---|
| 1 | push가 RAM에 써지지 않음 | 덤프의 스택 영역 | 처음 덤프는 범위가 일부만 있었음. 전체 덤프에서는 스택이 정상 | 기각 |
| 2 | RAM이 byte 접근을 지원하지 않아서 생김 | calc/float/io 소스 검사, 에뮬레이터 엄격 검사 | 메모리 접근은 모두 `32B`. `S16H`/`16L`은 `set`에만 사용 | 기각 |
| 3 | 짧은 call → ret만으로 재현 | `1` (call r0 → jmp → call r0), `2` (mul10n 모양 kz) | 6/6 통과 | 기각 |
| 4 | str2f 구간을 그대로 옮기면 재현 | `3` (set32 복사본 → kz → jmp → call r0), `4` (실제 `fp_str2f("5")`) | 3: 첫 실행에서 재시작, 이후 4/6, 2/6 실패. 4: 3/6 실패 | 재현됨 |
| 5 | set32 하나로 충분 | `5` (set32만) | 6/6 통과 | 기각. set32 직후의 call → ret이 필요 |
| 6 | kz 모양이 필요 | `7` (kz 대신 r0) | 2/6 실패 | 기각 |
| 7 | 마지막 `call r0`이 필요 | `9` (kz로 끝) | 2/6 실패 | 기각 |
| 8 | 끝 store 2개가 필요 | `6` (bzero만 + kz), `k` (중첩 유지, 끝 store 제거) | 둘 다 6/6 통과 | 필요함 |
| 9 | 중첩 call(set32 → bzero)이 필요 | `8` (store를 inline), `l` (bzero + 끝 store, 중첩 없음) | 둘 다 6/6 통과 | 필요함 |
| 10 | ret이 다른 store 데이터를 포워딩받음 | `j` (store 데이터를 모두 trap 주소로, 즉시값 `set`) | 6/6 통과, trap 없음 | 보류. 데이터를 만드는 방식(즉시값 / ALU 체인)도 바뀜 |
| 11 | `zero.32B`로 store하는 것이 원인 | `n` (0을 담은 일반 레지스터로 store) | 연속 0/6, m과 번갈아 5/6 실패 | 기각 |
| 12 | 끝 store의 값(0/1)이 원인 | `m` (끝 store를 같은 ALU 체인으로 trap 주소로) | 0/12 실패 | 보류. 명령이 3개 늘어 id 정렬이 바뀜 |
| 13 | 직전 실행 이력(id/epoch 정렬)에 따라 달라짐 | n 연속 실행과 m과 번갈아 실행 비교 | 0/6 대 5/6 | 유력 |

### 재현 조건

1. 중첩 call: `call set32` → `call bzero`
2. set32 안의 끝 store 2개. 데이터는 ALU 체인(`mov` → `shiftri` → `bndi`)으로 만든 값
3. set32가 복귀한 직후의 짧은 call → ret. 이 ret은 앞의 `call set32`가 push한 것과 같은 스택 슬롯을 pop합니다

- 같은 코드라도 이전에 무엇을 실행했는지에 따라 실패율이 크게 달라집니다. 명령 id나 epoch 정렬이 관련 있어 보이며, 위의 원인 2(flush에서 `CLR_inst`가 비는 현상)와 같은 문제일 수 있습니다.
- 남은 후보:
  - (a) store buffer 포워딩이 잘못된 entry를 고름. 끝 store의 데이터 0이 ret으로 들어가 주소 0으로 점프
  - (b) 타이밍이나 id 정렬에 따른 flush 처리 오류
  - (c) 데이터 값 0을 특별하게 다루는 로직

### 다음 실험: `o` / `p` (빌드 완료, 회로 결과 대기)

코드와 commit되는 명령 수를 같게 맞추고, 끝 store의 데이터만 다르게 했습니다. `gen[9]`에 미리 값을 넣고 `mov gen[2], gen[9]`로 씁니다.

- `o`: 1, 0을 씁니다(테스트 3과 같음).
- `p`: trap 주소 `0x3D0`(trap0), `0x3E4`(trap4)를 씁니다.
- 실행 방식: o 연속 6번, p 연속 6번, o/p 번갈아 6번.

| 결과 | 의미 |
|---|---|
| p도 재시작 | 값은 원인이 아님. 타이밍이나 id 정렬 문제이므로 flush와 `CLR_inst` 확인 |
| p가 `!0` / `!4` 출력 | store buffer 포워딩이 잘못된 entry를 고른 것이 확정 |
| o만 실패 | 데이터 0이나 1을 특별하게 처리하는 로직을 찾아야 함 |

- 회로 trace에서 잡을 신호: ret(pop)의 load 데이터와 포워딩 entry 선택, store buffer 내용, id-manager의 `clr_id`/`clr_ep`/epoch, flush 신호.

## 원인 3: flush와 같은 반클럭에 dispatch된 stack 연산의 SP 갱신이 버려짐 (확정·수정됨, 2026-10-11)

calc 재시작(memtest8 `3`/`4`/`7`/`9`/`m`/`n`/`o`/`p`)의 원인입니다. 회로를 headless로 직접 시뮬레이션해서 찾았습니다(아래 "방법"). 후보 (a) 포워딩, (b) id-manager epoch, (c) 값 0 특별 처리는 **모두 아닙니다.**

### 결론

- 추측 SP 레지스터 SCS는 stack 연산(call/ret/push/pop)이 **RS에서 dispatch될 때**(`inst_DP`, `CT_DISP`) ±4로 갱신됩니다. flush가 오면 ROB-SC에서 읽은 복구값으로 덮어씁니다.
- 갱신 mux가 `scs_i = clr ? FD : SCS ± SCN`입니다(`Multiplexer@2680,-160`, sel=`clr`). 그래서 **flush 반클럭(`clr`=1)에 dispatch된 stack 연산의 ±4가 버려집니다.** 그 연산이 flush보다 오래되어 살아남는 명령이어도 마찬가지입니다.
- 같은 반클럭에 그 연산의 ROB-SC 기록(`D = scs_i`)도 갱신 전 값(FD)으로 들어갑니다. 그래서 이후 그 연산 자신의 flush로 복구해도 바로잡히지 않습니다.
- 재현 경우(test 3, 첫 실행): `call kz`(0x268)의 −4가 사라져 SCS가 0x19DC로 남습니다. kz의 `ret`(0x568)이 **0x19DC**(stack 끝 바깥, 값 0)를 pop해서 주소 0으로 점프합니다. 이때 SP는 0x19E0이 됩니다.
- 반대 방향도 있습니다. `ret`이 같은 상황이면 +4가 사라져 SP가 4 낮아집니다. memtest8 `k`는 재시작은 안 하지만 SP가 실행마다 19DC → 19D8 → 19D4로 샙니다(아래 표). 앞의 가설 4(selftest W 뒤 SP가 8 word 어긋남)도 같은 계열로 보입니다.

### 해당 구간 인코딩 (memtest8.o + float.o 링크, `dlnk memtest8.o float.o`)

`[op][a][b|imm hi][imm lo]`. 레지스터 바이트 = id<<3 | mode (`D0` = zero.32B, `08` = gen[1], `40` = gen[8], `68` = gen[13], `90` = gen[18]).

| 라벨 | 주소 | 워드 | op | a | b/imm | 명령 |
|---|---|---|---|---|---|---|
| t3 | 0x248 | `1A400000` | 1A | 40 | 0000 | `set gen[8].32B, 0x0` |
|  | 0x24C | `1A0817A4` | 1A | 08 | 17A4 | `set gen[1].32B, 0x17a4` (bigB) |
|  | 0x250 | `1A100001` | 1A | 10 | 0001 | `set gen[2].32B, 0x1` |
|  | 0x254 | `70D0058C` | 70 | D0 | 058C | `call zero.32B, set32` |
|  | 0x258 | `17400000` | 17 | 40 | 0000 | `cmpi gen[8].32B, 0x0` |
|  | 0x25C | `61D00270` | 61 | D0 | 0270 | `jmp.neg zero.32B, t3_kneg` |
|  | 0x260 | `1A0817A4` | 1A | 08 | 17A4 | `set gen[1].32B, 0x17a4` |
|  | 0x264 | `1C104000` | 1C | 10 | 4000 | `mov gen[2].32B, gen[8].32B` |
|  | 0x268 | `70D00550` | 70 | D0 | 0550 | **`call zero.32B, kz`** ← −4가 사라지는 명령 |
|  | 0x26C | `66D00280` | 66 | D0 | 0280 | **`jmp.one zero.32B, t3_div`** ← 같은 반클럭에 flush하는 명령 |
|  | 0x270–0x27C | | | | | t3_kneg (실행 안 됨) |
|  | 0x280 | `70D0054C` | 70 | D0 | 054C | `call zero.32B, r0` |
|  | 0x284 | `66D00218` | 66 | D0 | 0218 | `jmp.one zero.32B, show_sp` |
| r0 | 0x54C | `71000000` | 71 | 00 | 0000 | `ret` |
| kz | 0x550 | `1C901000` | 1C | 90 | 1000 | `mov gen[18].32B, gen[2].32B` |
|  | 0x554 | `17900000` | 17 | 90 | 0000 | `cmpi gen[18].32B, 0x0` |
|  | 0x558 | `60D00568` | 60 | D0 | 0568 | `jmp.zero zero.32B, kz_end` |
|  | 0x55C | `70D0054C` | 70 | D0 | 054C | `call zero.32B, r0` (wrong path) |
|  | 0x560 | `05900001` | 05 | 90 | 0001 | `subi gen[18].32B, 0x1` |
|  | 0x564 | `66D00554` | 66 | D0 | 0554 | `jmp.one zero.32B, kz_lp` |
|  | 0x568 | `71000000` | 71 | 00 | 0000 | **`ret`** ← 0x19DC를 pop해서 0으로 점프 |
| bzero | 0x56C | `1C680800` | 1C | 68 | 0800 | `mov gen[13].32B, gen[1].32B` |
|  | 0x570 | `1A70000E` | 1A | 70 | 000E | `set gen[14].32B, 0xe` |
|  | 0x574 | `4DD00000` | 4D | D0 | 0000 | `st.gen[13] zero.32B, 0x0` |
|  | 0x578 | `02680004` | 02 | 68 | 0004 | `addi gen[13].32B, 0x4` |
|  | 0x57C | `05700001` | 05 | 70 | 0001 | `subi gen[14].32B, 0x1` |
|  | 0x580 | `17700000` | 17 | 70 | 0000 | `cmpi gen[14].32B, 0x0` |
|  | 0x584 | `68D00574` | 68 | D0 | 0574 | `ijmp.zero zero.32B, bzero_lp` |
|  | 0x588 | `71000000` | 71 | 00 | 0000 | `ret` |
| set32 | 0x58C | `70D0056C` | 70 | D0 | 056C | `call zero.32B, bzero` |
|  | 0x590 | `1C681000` | 1C | 68 | 1000 | `mov gen[13].32B, gen[2].32B` |
|  | 0x594 | `0B68FFFF` | 0B | 68 | FFFF | `bndi gen[13].32B, 0xffff` |
|  | 0x598 | `41680000` | 41 | 68 | 0000 | `st.gen[1] gen[13].32B, 0x0` |
|  | 0x59C | `1C681000` | 1C | 68 | 1000 | `mov gen[13].32B, gen[2].32B` |
|  | 0x5A0 | `15680010` | 15 | 68 | 0010 | `shiftri gen[13].32B, 0x10` |
|  | 0x5A4 | `0B68FFFF` | 0B | 68 | FFFF | `bndi gen[13].32B, 0xffff` |
|  | 0x5A8 | `41680004` | 41 | 68 | 0004 | `st.gen[1] gen[13].32B, 0x4` (끝 store, id D) |
|  | 0x5AC | `71000000` | 71 | 00 | 0000 | `ret` (id E) |

스택: `call set32`가 0x19D8에 0x258을 push하고, `call bzero`가 0x19D4에 0x590을 push합니다. `stk_end` = 0x19DC이고, 그 위 0x19DC는 스택 밖이라 값이 0입니다.

### 회로 경로 (`cpu-32 test.dig`, 넷 이름과 소자 위치)

| 부분 | 내용 |
|---|---|
| SCS 갱신 | `regFile4.SCS_i` ← `scs_i` = `Multiplexer@2680,-160`(sel=`clr`, in_0 = `addsub-4c32@2560,-180`.s, in_1 = `CT_SC-_FD`). adder.a = `Multiplexer@2460,-180`(sel=`CT_ISCCM`: SCS / ISC), b = `CT_RF-_SCN`(4), sub = `CT_RF-_SCD`(1=push/call) |
| 갱신 enable | `CT_RF-_SCSE`/`CT_RF-_SCSS`는 RS dispatch 쪽 Driver(@2400,-2940 / -2140, @1540,-1120)가 켭니다. flush 때는 `Driver@2100,-160/-120`(sel=`clr`, in=VDD)이 강제로 켭니다 |
| ROB-SC 기록 | `ROB-SC.D` = `scs_i`, `inst` = `inst_DP`(RS dispatch id), `E` = `CT_RF-_SCSRE`(stack 연산 dispatch 때만 1). 항목 = {SP, valid} |
| 복구값 FD | `ROB-SC.D3` = `roll`(=`clr_id`) 이하에서 가장 젊은 유효 항목. 없으면 SCA(commit된 SP) |
| commit | `W1/D1`, `W2/D2` → `Multiplexer@2540,-20` → `sca_i` |
| dispatch 순서 | RS의 `SC` 태그: stack 연산은 **바로 앞 stack 연산이 commit된 뒤에야** dispatch됩니다 |

`issue_e`(`And@320,-200`)는 decode→RS 삽입 enable입니다. RS dispatch나 SC enable은 이 신호로 막히지 않습니다(시험해서 확인).

### 실패 경과 (headless 재현, 키 `1`,`2`,`3` 순서, 반클럭 번호)

| 반클럭 | 사건 | SCS |
|---|---|---|
| 2205 | set32 `ret`(id E) dispatch: SCS +4, ROB-SC[E]=19DC | 19D8→19DC |
| 2219 | `ret` E resolve → flush, 0x258로 | 19DC |
| 2233 | 끝 store(id D) dispatch. 데이터 `mov→shiftri→bndi`가 이제 준비됨 | |
| 2237 | D, E commit (`com1_inst`=D, `com2_inst`=E). ROB-SC[E] 지워짐, SCA=19DC. 같은 때 `jmp.one`(0x26C, id 4) dispatch. BTB에 없어서 fall-through로 예측됨 | 19DC |
| **2239** | `call kz`(id 3) dispatch: SCSE=1, SCN=4, SCD=1. **같은 반클럭에 `jmp.one` flush**(`clr`=1, roll=4). ROB-SC에 4 이하 항목이 없어서 FD=SCA=19DC. mux가 FD를 골라서 **−4가 버려짐**. ROB-SC[3]=19DC로 기록. push 자체(store 0x26C → 0x19D8)는 정상 | 19DC (정상이면 19D8) |
| 2241 | `call kz` 자신의 flush(roll=3): FD=ROB-SC[3]=19DC | 19DC |
| 2257–2268 | kz 안 wrong-path `call r0`가 −4를 했다가 `jmp.zero` flush로 19DC로 복구됨 | 19DC |
| 2275 | kz `ret`(0x568, id 7) dispatch: pop 주소 = **0x19DC**, SCS +4 | 19DC→19E0 |
| 2285–2289 | load 데이터 0 (`LDB_forward`=0, 메모리 값) → `JPU_RET`, `JPU_ADDR`=0 → flush로 PC=0 | |

### 세 조건이 모두 필요한 이유

- stack 연산은 앞 stack 연산이 commit된 뒤에야 dispatch됩니다. 그래서 `call kz`는 set32의 `ret`(id E)이 commit되기를 기다립니다.
- (2) 끝 store 2개의 데이터는 `mov→shiftri→bndi` 체인으로 늦게 준비됩니다. in-order commit이라 `ret` E도 그 뒤에야 commit됩니다. 그 결과 `call kz`의 dispatch가 2239까지 밀리고, 더 젊은 `jmp.one`(2237 dispatch)의 resolve와 정확히 겹칩니다.
  - `j`(즉시값 `set`)에서는 데이터가 일찍 준비되어 call이 먼저 나가므로 겹치지 않습니다.
- (1) 중첩: set32의 `ret`이 끝 store **뒤에** 있어야 commit이 늦어집니다.
  - `l`(중첩 없음)에서는 call 앞 stack 연산이 bzero의 `ret`입니다. 이 ret은 끝 store보다 먼저 commit되므로 call이 일찍 나갑니다.
  - `8`(inline)도 같습니다.
- (3) call 직후 BTB에 없는 분기(`jmp.one`)가 있어야, 그 flush가 call의 dispatch 반클럭에 걸립니다.
  - 마지막 `call r0`는 필요 없습니다. 충돌은 `call kz`와 `jmp.one` 사이에서 일어나고, 0으로 가는 것은 kz의 `ret`입니다.
- 이력 의존: 같은 `3`이라도 키 `3`만 처음 누르면 12/12 통과하고, `1`,`2` 뒤에 누르면 첫 번에 실패합니다.
  - 겹침 여부는 반클럭 단위 정렬과 BTB 상태에 달려 있습니다. BTB에 `jmp.one`이 있으면 flush가 없습니다.
  - BTB가 16 entry이고 index가 4비트라 다른 테스트와 alias가 납니다. 그래서 n 연속 실행과 m/n 번갈아 실행의 실패율이 다르게 나온 것으로 봅니다.

### headless 재현 결과 (원본 회로)

| 입력 | 결과 |
|---|---|
| `1 2 3` | 3에서 재시작 (위 경과) |
| `333333` | 3번째에 재시작, 4·5번째 재시작, 6번째 통과 |
| `oooooo` / `pppppp` / `opopop` | 셋 다 첫 실행에서 재시작. **p도 재시작**하고 `!0`/`!4`는 한 번도 안 나옴 → 데이터 값이나 포워딩은 무관 |
| `mnmnmnmn` | n에서 2회 재시작 |
| `kkk` | 재시작 없음. SP 19DC → 19D8 → 19D4 (`ret`의 +4가 같은 방식으로 두 번 버려짐, 반클럭 1950/3088) |
| `jjj`, `lll` | 통과 |
| calc `5/2` | 재시작 (원래 증상) |

### 수정안과 검증

- **제안**: flush 반클럭에 dispatch된 stack 연산은 FD 위에 ±4를 적용합니다.
  - 즉 mux를 adder 앞으로 옮겨서 `scs_i = (clr ? FD : (ISCCM ? ISC : SCS)) ± SCN`로 바꿉니다.
  - stack 연산이 없으면 SCN은 PullDown으로 0이므로, 지금처럼 FD만 들어갑니다.
  - ROB-SC 기록도 같은 `scs_i`를 쓰므로 함께 바로잡힙니다.
- headless 패치(`tools/dsim/Patch.java`의 `scsfd`)로 확인했습니다.
  - memtest8 `1`–`9`, `j`–`p`: 전부 SP 000019DC.
  - `333333`, `kkk`, `abcdefghi`(자동 반복), `4444`(실제 `fp_str2f`, 40A00000 4/4): 모두 통과.
  - calc `5/2` = 2.5, `10/3` = 3.333333.
- **확인할 점**: 이 수정은 "flush 반클럭에 dispatch된 stack 연산이 flush보다 오래된 명령"이라고 가정합니다.
  - 측정한 충돌 7건(패치판 memtest8 5건, 원본 calc 2건)은 모두 오래된 명령이었습니다(dispatch id가 `CLR_inst`에 없음).
  - 하지만 더 젊은 명령이 그 반클럭에 dispatch될 수 있다면 ±4를 잘못 더하게 됩니다.
  - 그 경우까지 막으려면 SCN(또는 `CT_RF-_SCSRE`)을 "dispatch id가 clr 범위 밖"(`inst-comp`)으로 gate하거나, flush 반클럭에는 RS가 stack 연산을 dispatch하지 않게 하면 됩니다.

### 남은 문제 (별개)

- **calc `1+2`가 패치판에서도 `^ error`**(커서 1, `+` 위치)로 나옵니다. → "원인 4"에서 원인을 찾았습니다.
- **시뮬레이션 비결정성**: 패치판 `abcdefghi`를 8번 실행했더니 1번 배너가 `m8 128456789a7i jklmnkp`로 깨졌습니다('3'→'8', '-'→'7', 'o'→'k').
  - 키 입력 전 구간이라 프로그램 입력과는 무관합니다. 같은 회로와 같은 입력인데 JVM 실행마다 결과가 다릅니다.
  - Digital의 노드 평가 순서에 따라 결과가 바뀌는 race가 회로 어딘가에 있다는 뜻입니다.
  - GUI에서 같은 테스트의 결과가 실행마다 달라지는 데에도 일부 기여할 수 있습니다(미조사).

## 원인 4: RS의 FL/SC 의존 태그가 commit과 같은 반클럭에 삽입된 새 생산자를 잃음 (확정·수정됨, 2026-10-11)

calc `1+2` → `^ error`의 원인입니다. 원인 3을 수정한 현재 회로(`addsub@2640,-160` → `scs_i`)에서 확인했습니다.

### 증상 경로 (calc `1+2`, parse_expr 0xEC)

| 주소 | 명령 | 회로 동작 |
|---|---|---|
| 0x100 | `ld.gen[12] gen[6], 0` | `+`(0x2B)를 읽음. 데이터는 59334에 도착 |
| 0x104 | `cmpi gen[6], '+'` (id B) | load를 기다리므로 **59336에 dispatch** |
| 0x108 | `jmp.zero 0x114` (id C) | **59326에 dispatch**(cmpi보다 먼저). 이전 flag(FL=0x54, zero=0)를 읽고 not-taken |
| 0x10C/0x110 | `cmpi '-'` / `ijmp.zero 0x140` | `-`가 아니므로 0x140(parse_expr 복귀) → 커서 1에서 에러 |

에뮬레이터와 회로의 commit store 열을 비교하면 store #681(0x1EB8 ← 0x100 다음)에서 처음 갈라집니다.

### 원인

- RS(`res-station-debug`)는 rename 표 `inst-wfile`에 flag 생산자 태그(FL, 31번 항목)와 stack 연산 태그(SC, 30번 항목)를 둡니다. 형식은 6비트 `{id, epoch, none}`입니다.
- 이 태그의 갱신 mux(`inst-wfile`: FL = `Multiplexer@2420,-360`, SC = `Multiplexer@2340,-620`)는 sel이 `FL_M`/`SC_M`(현재 태그가 이번에 commit됨)이면 **무조건 "없음"(또는 rcom_id)을 고릅니다.** 같은 반클럭에 삽입되는 새 생산자 id(`FL_i`/`SC_i`)는 버려집니다.
- 59321: `cmpi '+'`(id B, OFL=1)가 삽입되는 바로 그 반클럭에 이전 태그 9가 commit됩니다(`FL_com2`). FL이 B가 아니라 없음(0x10)이 됩니다.
- 59323: `jmp.zero`(id C, UFL=1)는 의존할 생산자가 없다고 보고 즉시 dispatch됩니다.
- 일반 레지스터 R1–R25는 같은 상황(삽입 대상 = commit 대상)이 수천 번 있어도 태그를 잃지 않습니다. **FL과 SC만** 문제입니다.
  - FL과 SC는 RC 쓰기(`RW_sel` decode) 대신 `OFL`/`OSC` 경로로 쓰는데, M 우선 처리가 이 경로를 고려하지 않습니다.
- `1+2` 한 번 실행 중 FL 충돌 557건 중 220건, SC 충돌 158건 중 33건에서 태그를 잃었습니다(리셋 이후, 반클럭 정렬 기준).
  - 대부분은 뒤따르는 소비자가 늦게 와서 해가 없습니다.
  - 생산자가 load 같은 긴 의존을 기다릴 때만 틀린 결과가 나옵니다. 그래서 `-`, `*`는 우연히 통과했습니다.
- SC 태그 손실은 "stack 연산은 앞 stack 연산 commit 뒤 dispatch" 규칙을 깨뜨립니다. 다음 stack 연산이 기다리지 않게 되어 call/ret OoO가 생길 수 있습니다.
  - 이번 실행에서는 stack dispatch 326건 중 실제 OoO는 0건이었습니다. `1+2` 에러의 직접 원인은 FL 쪽입니다.

### 수정안과 검증 (`tools/dsim/Patch.java` `wfprio`)

1. `inst-wfile`: FL/SC data mux의 sel을 `X_M & !(X_sel & !X_i[5])`로 바꿉니다. 실제 삽입이 commit-clear보다 우선합니다.
2. RS: `FL_inst`/`SC_inst` mux(`Multiplexer@-3500,1860`/`-3500,1800`)의 sel을 `OFL`/`OSC` → `OFL/OSC & E & !clr`(`And@-3340,1540` 출력)로 바꿉니다.
   - 삽입이 stall된(E=0) 반클럭에도 OSC=1이면 `SC_i`에 id가 실립니다. 2를 빼고 1만 적용하면, 아직 삽입되지 않은 명령을 태그로 써서 자기 자신을 기다리는 데드락이 생깁니다(calc 시작 직후 commit이 id 3에서 멈춤).

결과:
- calc `1+2`=3, `5/2`=2.5, `1.5 * (2 + -0.5) / 4`=0.5625, `10/3`=3.333333, `2-1`=1, `2*3`=6.
- memtest8 `1`–`9`, `j`–`p`, `333333`, `kkk`, `abc`, `4444`: 모두 SP 000019DC, str2f 40A00000.
- 패치판 `1+2` 실행에서 FL/SC 태그 손실 0건.

## 원인 5: front-end stall 중 flush가 오면 wrong-path 명령이 살아남아 RS에 삽입됨 (확정·수정됨, 2026-10-11)

calc 곱셈 `inf`(`(1/3)*1.4` 등)의 원인입니다. 원인 3, 4를 수정한 현재 회로(`inst-wfile`, `res-station-debug` 수정 반영)에서 확인했습니다. `1.4*2`는 정상입니다.

### 증상 경로

- 레지스터 변화 열을 에뮬레이터와 비교하면 `fp_pack`(0x580~)에서 처음 갈라집니다.
  - 정규화 루프: 0x588 `cmp gen[15], 0x40000000` / 0x58C `ijmp.neg 0x59C` / 0x590 `shiftli` / 0x594 `subi gen[14]` / 0x598 `jmp.one 0x588`.
  - 루프를 빠져나온 뒤 0x59C `cmpi gen[14], 0xFF` / 0x5A0 `ijmp.neg 0x634`(exp ≥ 255 → inf)가 옵니다.
- 회로에서는 루프가 한 번 더 도는 중에 0x5A0 `ijmp.neg 0x634`(id 1.2)가 끼어듭니다. 이 분기는 `cmpi`(0x59C) 없이 이전 flag로 taken이 되어 0x634로 가고, 결과가 inf(0x7F800000)가 됩니다.

### 원인 (반클럭)

| 반클럭 | 사건 |
|---|---|
| 199098 | `jmp.one`(0x598, id 1.1)이 RS에 삽입됨. 그 뒤 front-end에는 fall-through로 fetch한 0x59C `cmpi`(DC)와 0x5A0 `ijmp.neg`(FT)가 있음 |
| 199099–199104 | 빈 id가 없어서(`id_full`=1) front-end stall: `halt-FT-DC-ISS`=1, `dc_e`=`ft_e`=0 |
| 199101–199102 | `jmp.one`이 taken으로 판정되어 flush(`clr`=1). PC는 0x588로 redirect됨 |
| 199103 | DC/ISS valid는 한 클럭 0이 됨 |
| 199105 | stall 해제. **DC valid가 다시 1**이 되지만(FT=1), DC/FT의 명령 워드는 stall 때문에 갱신되지 않은 flush 이전 값 |
| 199107–199108 | flush 이전에 fetch한 0x5A0 `ijmp.neg 0x634`가 ISS를 거쳐 **새 id 1.2로 RS에 삽입됨**(199148 dispatch 워드 `69D00634`로 확인) |
| 199148–199150 | 이 분기가 이전 flag로 taken → 0x634로 flush. 정상 경로의 루프가 지워짐 |

회로 구조:
- `RS_FF 'FT'@-160,-240`: S = `run | run_int`, R = `wait`. 실행 중 플래그일 뿐 `clr`로 지워지지 않습니다.
- `Register 'DC'@60,-260`(D = `FT & NOr(clr, wait)`), `Register 'ISS'@220,-260`(D = `DC & NOr(clr, wait)`): `en`=VDD라서 매 클럭 다시 계산됩니다.
- 명령 워드 레지스터(DC 데이터, fetch 결과)는 `dc_e`/`ft_e`로 막혀 stall 동안 그대로입니다.
- stall이 없을 때는 flush 다음 클럭에 데이터 레지스터도 새 PC의 fetch로 갱신되므로 문제가 없습니다. **stall과 flush가 겹칠 때만** valid가 옛 워드보다 먼저 되살아납니다.

### 수정 방향 (사용자가 회로에 반영, "수정 후 검증"에서 확인)

- 단계마다 "이 워드가 유효한가"를 데이터 레지스터와 같은 enable로 붙여야 합니다.
  - FT 단계: fetch 결과 워드의 valid. `clr`이면 0, 새 PC의 fetch가 들어오면(`ft_e`) 1, 그 외에는 유지.
  - DC/ISS 단계: `clr`이면 0, `dc_e`(stall 해제)일 때만 앞 단계 valid를 받고, 그 외에는 유지.
- 즉 지금처럼 `en`=VDD로 매 클럭 다시 계산하지 말고, `clr` 우선 + stall 시 유지로 바꿔야 합니다.
- 원인 2(M4-7.csv)에서 flush 뒤에 front-end 명령(`bnd`/`cmpi`/`jmp.neg`)이 dispatch된 현상도 같은 원인일 가능성이 큽니다.

## 수정 후 검증 (2026-10-11, 원인 3–5 회로 반영 후)

`cpu-32 test.dig`(`inst-wfile`, `res-station-debug`, front-end valid 수정 반영)를 headless(`tools/dsim/Sim`)로 실행했습니다.

calc (`program/run.txt` = calc.o float.o io.o):

| 입력 | 회로 | 에뮬레이터 |
|---|---|---|
| `(1/3)*1.4` | 0.4666667 | 0.4666667 |
| `1.4*2` | 2.8 | 2.8 |
| `2*2*0.66*1.33*0.8` | 2.80896 | 2.80896 |
| `2*(2/1)*(2/3)*(4/3)*(4/5)` | 2.844445 | 2.844445 |
| `1+2` | 3 | 3 |
| `5/2` | 2.5 | 2.5 |
| `10/3` | 3.333333 | 3.333333 |
| `1.5 * (2 + -0.5) / 4` | 0.5625 | 0.5625 |

memtest8 (memtest8.o float.o):

| 입력 | 결과 |
|---|---|
| `1`–`9`, `j`–`p` | 모두 SP 000019DC, `4` → 40A00000 |
| `333333` | 6/6 정상 |
| `kkk` | SP 누수 없음 |
| `4444` | 40A00000 4/4 |

남은 확인: GUI 실행, selftest T–X, memtest4 test 7(원인 2), 앞서 한 번 본 시뮬레이션 비결정성(배너가 깨짐, 1/8).

### 방법: headless 시뮬레이터 (`tools/dsim/`)

Digital.jar를 라이브러리로 써서 `cpu-32 test.dig`를 GUI 없이 돌립니다. 결정적 반클럭 단위로 모든 probe와 tunnel 넷을 CSV로 덤프할 수 있습니다. 사용법은 `tools/dsim/README.md`에 있습니다.

- `Sim.java`: 프로그램 preload, 클럭(DipSwitch @-580,-1640 켜서 Clock 선택), `E`/`run`=1, 키보드와 터미널 연결, 재시작 감지. `-Dtap=top;ROB-SC.dig;...`로 tunnel마다 probe를 붙이고 `-Dcsv`로 덤프합니다.
- `NetQuery.java`: 넷 이름이나 좌표로 연결된 소자와 핀을 출력합니다(위 "회로 경로" 표를 이것으로 만들었습니다).
- `Patch.java`: 메모리 안에서만 회로를 고쳐 시험합니다(`-Dpatch=scsfd`). `.dig` 파일은 건드리지 않습니다.
- `emutrace.py`: 에뮬레이터로 동적 명령 trace(주소, 워드, 역어셈블, SP)를 출력합니다.

## 관련 파일

| 파일 | 용도 |
|---|---|
| `program/test/selftest.asm` | 명령별 검사와 float 라이브러리 호출 |
| `program/test/memtest.asm` ~ `memtest6.asm` | 메모리 순서 실험 (memtest3 이후는 키로 테스트 선택) |
| `program/calc.asm`, `floatingpoint/`, `io/` | 원래 프로그램과 라이브러리 |
| `log/raw/M4.csv`, `log/compare/M4SUC.csv`, `log/compare/M4ERR.csv` | test 2 덤프 원본과 분할 파일 |
| `log/compare/COMP.csv` | M4SUC/M4ERR 분기점 ±20행 SUC/ERR 교차 비교 (Excel용) |
| `log/raw/M4-7.csv` | test 7 덤프 (원인 1 수정 후) |
| `log/ram/calc-dump.txt` | calc 재시작 후 RAM 덤프 |
| `program/test/memtest8.asm` | calc 재시작 재현 실험 (키 1–9, a–i 자동 16회, j–p) |
| `tools/dsim/` | headless 시뮬레이터(Sim), 넷 조회(NetQuery), 메모리 안 패치(Patch), 에뮬레이터 trace(emutrace.py) |
