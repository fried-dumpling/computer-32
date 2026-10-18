# TODO

> `todo.txt`를 정리한 문서입니다(원본은 그대로 둠). 상태는 2026-10-18 기준 저장소 내용과 대조했습니다.
> - `[x]` 완료 · `[~]` 진행 중 / 부분 완료 · `[ ]` 미착수 · `(?)` 코드로 확인되지 않아 직접 확인 필요
> - 이 탭에는 해야 할 일만 둡니다. 끝난 일과 설계 기록은 아래 [완료](#완료) 탭에 있습니다.

## 요약

| 영역 | 상태 | 비고 |
|---|---|---|
| 어셈블러 | 거의 완료 | `#include`, `#ifdef`, `#ifndef` 구현됨 (`tools/SYNTAX.md` §include/ifdef). 확장 레지스터 일부 미지원 |
| 링커 | 동작 | 다중 오브젝트 링크, 8바이트 헤더 / `-raw` / `-start`. jmp 축소 최적화는 미구현 (선택 사항) |
| ISA 정리 | 대부분 완료 (?) | `ISA.md`, `assembler.hpp`에 NMI / trap-flag 흔적 없음 |
| 인터럽트 | 진행 중 | `syscall`/`iret`/`intervec`/`interpri`/`ldexp`/`stexp`, `ROB-INT.dig` 배선 중 |
| 메모리 (ld/st buffer) | 대부분 해결 | queue → age matrix 전환 완료 (6b8b02b) |
| 메모리 바이트 주소 / 리틀엔디안 | 동작 | ld/st 폭, `set` 모드 회로 확인 (2026-10-18) |
| 외부 RAM / ROM, boot-sequence | 동작 | boot ROM → 외부 ROM의 프로그램을 RAM으로 복사 → 실행. add_terminal, io_harness, calc, picalc 회로에서 통과 (2026-10-18) |
| forwarding / PC-mux | 목표 설정 | 아래 "다음 목표" 참고 |
| IPU / MMU / cache / 멀티채널 RAM | 미착수 | |

---

## 다음 목표

### 1. Forwarding
FU 결과를 대기 중인 명령에 바로 넘겨서 ready가 되기까지 걸리는 사이클을 줄인다.
- [ ] 현재 결과 전달 경로 파악: FU → ROB/regFile → res-station까지 걸리는 사이클 측정
- [ ] FU 결과 버스(tag + data)를 res-station에 직접 broadcast하여 wakeup·operand capture
- [ ] dispatch 시점에 결과가 같은 사이클에 나오는 경우(regFile 읽기와 FU 결과 충돌) 우회 경로
- [ ] flush 시 forwarding 경로의 결과 무효화 (wrong-spec id보다 큰 결과 무시)
- [ ] 완료 기준: add_terminal / calc가 그대로 동작하고, 의존 명령 연속 실행 시 사이클 수 감소 확인
  - 기준값 (2026-10-18, headless sim): calc 4줄 147,564 명령 / 약 39만 사이클, picalc `0 20` 191,829 명령 / 약 57만 사이클 (IPC 약 0.35~0.38)

### 2. PC-mux 최적화
- [ ] 현재 next-PC 소스 정리: PC+4, BTB 예측 target, RAS(ret), JPU 오예측 수정 target, flush/rollback PC, 인터럽트 벡터, (boot reset vector)
- [ ] 우선순위 정리 (예: reset > 인터럽트 > flush/rollback > RAS > BTB > PC+4)
- [ ] 우선순위 인코더 + 단일 mux 구조로 재구성 — 지금처럼 mux를 직렬로 이으면 critical path가 길어짐
- [ ] 자주 쓰는 경로(PC+4, BTB)를 mux 마지막 단에 두어 지연 단축
- [ ] 완료 기준: 분기/call/ret/flush 테스트 통과, mux 단 수(gate 수) 감소

### 3. Boot-sequence | 외부 ROM
동작은 완료(아래 완료 탭). 남은 일:
- [~] reset 시퀀스: reset PC = boot ROM은 동작. 파이프라인·ROB·RAS·BTB 초기화는 따로 확인 안 함
  - [~] 부트 코드 `boot/src/boot.asm` → `boot/boot.bin`
    - [ ] stack / istack 설정, `intervec`·`panicvec` 설정
    - [ ] 머리 주석의 `org 0x3FFFFF` 정리

### 4. ISA ↔ 툴 맞추기 (`ISA.md` 6절)
- [ ] 어셈블러에 확장 레지스터 `int-level`(ex-4), `p-base`(ex-5), `p-limit`(ex-6) 추가 — 지금은 `cpuid`/`panicvec`/`istack`만 받음
- [ ] `rol`/`ror` 회로 동작 확인 (시트: `rol` = rollleft, `ror` = rollright), 에뮬레이터에 추가
- [ ] floatingpoint 테스트(`fp_test.py`)는 에뮬레이터 전용 — 회로에서 돌릴 방법 검토 (calc로 간접 확인만 됨)

---

## 부속별 정리

### 1. 어셈블러 / 링커
- [~] 어셈블러 마무리 (원문: "partially finished?")
- [ ] 복잡한 수식 사용 시 warning 출력?

선택 사항 (처음부터 구현하지 않기로 함):
- [ ] 링커 longjmp / autojmp 축소
- [ ] 더 이상 축소되지 않을 때까지 "수식 계산"부터 반복

### 2. CPU

#### ISA 정리
- [~] ISA 재배치

#### 인터럽트

##### 해야 할 일
- [~] `iret`
- [~] `syscall` — IPU가 해당 인터럽트를 가져갈 때까지 다른 인터럽트를 모두 마스크
- [ ] 외부 IPU 연결 (CPU에 인터럽트 구현하려면 필요)

##### 문제: 인터럽트 수락과 trap이 동시에 발생
- 둘은 동시에 실행될 수 없음 → 수락을 되돌리거나 trap을 취소해야 함
- 그런데 trap은 이미 commit됨 → 수락을 되돌려야 하는데, 수락 되돌리기는 불가능
- → 파이프라인에서 수락 시점을 늦춘다

**해결책**
> **IPU는 현재 보내고 있는 인터럽트를 수락될 때까지 바꾸지 않는다고 보장한다.** (중요!)

CPU 쪽:
- 내부적으로 수락 — 파이프라인에 약간의 지연이 필요할 수 있음
- trap이 commit되었으면 취소
- 다른 commit 여부를 실행 전에 확인하므로 미리 검사 가능
- "인터럽트 비마스크 + trap commit" 구간에 있는지 래치로 판별

##### `syscall` 동작
1. 인터럽트 마스크
2. trap 실행
3. commit 시 인터럽트 언마스크

##### `iret` 동작
- (미정)

##### 인터럽트 처리 흐름
1. **인터럽트 신호**
   - `C_dt` → `C_ct`에서 신호 버퍼링?
   - 레벨 확인
   - 레벨이 더 높고 마스크되지 않았으면 인터럽트 실행, 아니면 수락하지 않음
2. **인터럽트 실행**
   - 마스크 설정
   - 레벨, 주소 등 저장
   - flush
   - FDI(fetch/decode/issue) 정지
   - 마지막 commit 대기
3. **마지막 commit 완료 & store queue 비었을 때**
   - inst-id 획득
   - store queue에 적재
   - 벡터로 점프
   - FDI 정지 해제
   - 인터럽트 마스크 해제
4. **메모리 처리**
   - ISC에 push
   - 명령을 valid로 설정 (store-buffer가 처리)

#### memory buffer 잡일
- [ ] **주소 계산 유닛(ACU) 통합** — ld/st는 다른 명령보다 드무므로 ACU 공유
- [ ] **메모리 offset 계산 추가** — 5:3 compressor로 lookahead하여 offset 포함
- [ ] **버퍼 크기 증가** — ACU 통합 시 st-buffer busy가 lda를 막거나 그 반대가 생김 → load/store buffer를 6~8로

#### RAW table
- [ ] invalid flush (내용 비어 있음)

#### mmu 연결

### 3. IPU
- [ ] 인터럽트 사전 요청
- [ ] 인터럽트 순서 결정 — 현재 요청은 조정하지 않음?

### 4. MMU
- [ ] 처음 구현
   - [ ] base-limit 구현 (확장 레지스터 `p-base` / `p-limit`)
- [ ] 페이지 구현: 페이지 계산은 ld/st buffer 뒤에서 수행 → 버퍼 안에서 물리/가상 주소 분리

### 5. CACHE
- [ ] MMU 장착, 0~8은 index, 9~40은 MMU 검사 (256byte page)
- [ ] D-CACHE
- [ ] I-CACHE
- [ ] L2-CACHE

### 미구현 / 미래 계획
- [ ] DMA
- [ ] VPU
   - [ ] CACHE 연동
- [ ] FPU

### 기타 미착수
- [ ] multi channel RAM
- [ ] fetch queue 추가 검토

---

# 완료

끝난 일과 설계 기록입니다. 원문 `fin?(fixed?)` 아래 항목(해결됨 / 설계 기록)도 여기로 옮겼습니다.

## Boot-sequence | 외부 ROM (2026-10-17 ~ 18)
- [x] 외부 RAM (테스트용) — `components/ram-test.dig`
- [x] 외부 ROM — `components/rom-test.dig`. `ld1`이 `D_ST`에 연결돼 있던 버그 수정
- [x] 메모리 맵 확정 (`boot/src/boot.asm` 머리 주석)
  - RAM `0x00000000`, key/term `0x04000000`, display `0x08000000`, ROM `0xFC000000` (bit 31:22 == 0x3F0 디코딩)
  - boot ROM `0xFFFFFF00` (64 word, 명령 전용) — reset PC
- [x] 부트 코드: ROM의 dlnk 헤더(시작 주소, 길이)를 읽고 프로그램을 RAM으로 word 단위 복사, `block` 후 시작 주소로 점프
- [x] 툴 지원: `dlnk` 출력에 8바이트 헤더(시작 주소, 길이), `-raw`(헤더 없음), `-start <label>`. 진입 프로그램은 `global _start` + `.text` 첫 줄에 `_start:`
- [x] BTB 버그 수정: 예측 taken 분기가 target+4를 fetch하던 문제
- [x] 완료 기준 (add_terminal): 리셋 → boot ROM → ROM의 add_terminal을 RAM으로 복사 → 실행. 12+34=46, 999+1=1000, 65535+65535=131070 확인 (2026-10-17, headless sim)
- [x] 완료 기준 (calc): 같은 경로로 회로에서 실행, 출력이 에뮬레이터와 같음 (2026-10-18)

## 메모리 폭 (바이트 주소 / 리틀엔디안)
회로 측정. 자세한 내용은 `ISA.md` 3절.
- [x] 메모리는 바이트 주소 + 리틀엔디안, 비정렬 접근 가능 (2026-10-17)
- [x] `ld`는 레지스터 모드(`D_MD`)로 폭 결정: `32B`/`16L`/`8L`/`8H`. 부분 load는 나머지 비트 유지 (2026-10-17)
- [x] store가 `D_MD`를 항상 0으로 내보내던 버그 수정 — `st .8L`/`.8H`는 1바이트, `.16L`은 2바이트만 씀 (2026-10-18 재확인)
- [x] `set`의 모드 동작 확인: 모드의 비트만 쓰고 나머지 유지. `S16L`/`S8L`/`S8H`는 `16L`/`8L`/`8H`와 같음 (2026-10-18). 에뮬레이터도 같게 수정

## 소프트웨어 (2026-10-17 ~ 18)
- [x] 문자열을 전부 바이트 문자열로 전환 (1 바이트 = 1 글자, 0으로 끝남): `io`, `floatingpoint`, `calc`, `picalc`, `test/selftest`
  - `io_printb`/`io_printlnb` 삭제, 바이트 읽기 매크로 `IO_LDB` / `FP_LDB`
  - 에뮬레이터 테스트: io_test, fp_test, pi_test 모두 ALL PASS
- [x] 에뮬레이터를 `tools/emu/isa32emu.py`로 이동, 리틀엔디안 / 바이트 주소 / dlnk 헤더 지원
- [x] Digital 회로에서 실행 (boot ROM → 외부 ROM → RAM, headless sim, 2026-10-18) — 출력이 에뮬레이터와 모두 같음
  - io_harness: 백스페이스, 넘치는 입력, `'\r'` 포함 전체 출력 일치
  - calc: `1.5*(2+-0.5)/4` = 0.5625, `2/3` = 0.6666667, `1+*2` 오류 위치, `q`
  - picalc: `0 20` → `3.` / `0: 1415926535 8979323846`

## 어셈블러 / 링커
### 어셈블러
- [x] `#include`
- [x] `#ifdef`
- [x] `#ifndef`

### 링커
원문 당시 "stack machine not ready" → 현재 `tools/linker/stackmachine.hpp` 존재, `dlnk` 동작.

- [x] 오브젝트 읽기
- [x] 식별자(label / const) 로드
- [x] 섹션 합치기
- [x] bss 확장 (`-padbss`)
- [x] 섹션 타입별 정렬
- [x] 수식 계산
- [x] 변수 위치 지정 / 매핑
- [x] 바이너리 합치기
- [x] 결과 출력

## CPU
### ISA 정리
- [x] NMI 제거 (?)
- [x] trap-flag 제거 (?)

### 메모리 이슈
- [x] `FL_MEM`이 1사이클 길이라 생기는 오류 — 또 다른 랜덤 버그
  - 원인: LD-buff SR-latch 문제
  - 키보드는 클럭 ROM으로 입력, enable/disable도 클럭 동기 → start/input이 자동·동기화됨
  - 관련 커밋: d318db0 "fix ramdom loading bug"
- [x] ld/st 동작 불량 → queue 재구성 → **age matrix로 해결** (69c485a, 24df657, 6b8b02b)

> 메모리 쪽에 매우 큰 문제가 있었음 (원문 메모)

**age matrix 설계**
- store buffer: 4x1? / load buffer: 4x2? 또는 없음?
- load buffer는 엄격한 순서가 필요 없음 (어차피 CPU가 소비) → 가능한 것부터 처리
- store buffer는 엄격한 명령 순서 필요 → 명령 순서 비교? 예약 시 age matrix?
- 비트 의미: 현재 항목이 더 오래됐으면 1, 아니면 0 (편의상 자기 자신은 1)
- id가 retire되면: 모든 모듈에서 그 id 라인을 1로 (동시에 들어오는 새 id는 제외)
- 새 id가 예약되면: 해당 모듈의 자기 자신 외 모든 id를 0으로

### LD / ST buffer
- [x] 메모리 접근 가능 여부 검사와 commit 지연용 ld/st buffer
- [x] st buffer에서 저장된 데이터 forwarding용 읽기 기능

**LD buffer**
- 필드: `addr`(32b), `data`(32b), `Avalid`(주소 준비), `Dvalid`(데이터 준비), `inst-id` & `epoch`(비교용)
- issue 시: dispatch된 명령에 버퍼 할당
- 주소 로드 (일반 FU ld와 동일 구조): 해당 버퍼에 lda addr, `Avalid` = 1
- 데이터 로드: 해당 버퍼에 lda data, `Dvalid` = 1
- `Avalid`면 유효한 lda를 id 순서로 포트에 송출 (원래 store-buffer queue 사용 → 현재 age matrix)
- `Dvalid`면 유효한 lda를 id 순서로 ROB에 송출

**ST buffer**
- 필드: `addr`, `data`, `inst-id`, `valid` (점유 중이고 올바른 inst-id가 적재됨, dispatch 사이클에 설정)
- issue 시: dispatch된 명령에 공간 할당, `valid` = 1
- dispatch는 (현재로선) 한 번에 하나만
- data/addr 수신 시: 선택된 레지스터 세트에 기록, data/addr 동시 적재 가능
- commit 시: 해당 데이터 세트를 출력하고 버퍼에서 제거
- 엣지 케이스: load / store가 동시에 버퍼에 push됨

### call & ret
- [x] **call synchronizer** — jmp와 stack을 dispatch, commit 동기화
- [x] **SCS rollback** — 이중 인코더 rollback 메커니즘 추가
- [x] **return sequencer** — stack을 ret 모드로 dispatch, stack 명령 완료 후 jmp dispatch
  - jmp dispatch는 2사이클 뒤에 일어나야 함
  - JPU에 busy 신호 전송 필요 → 타이밍 예측 가능
  - JPU 담당: `call` / `ret` / `jmp` / `ijmp`
  - 대안: JPU 유휴 시간 사용 (효율 저하)

**아이디어**: 현재 dispatcher를 uop-dispatcher로 업그레이드
- **채택**: dispatcher에 valid buffer를 하나 더 두어 2-uop 명령을 받도록 함
- 일반 1-uop 설계는 single mux / double gate 구조로 mux 수 절감

### ALU & 기타 FU
- [x] 명령 검증(instruction validator) 회로 — ALU 근처에 테스트용 배치, 동작하는 것으로 보임

### Return Address Stack
- [x] 4 모듈 speculative RAS
- [x] 8 모듈 architectural RAS

### jmp function unit (JPU)
- 점프 주소와 조건 계산 유닛
- 향후 여기에 분기 예측 추가 가능

### BTB & 분기 예측
- 크기 16
- 데이터: 2비트 예측기 (weak ← 0, 1, 2, 3 → strong), 12비트 PC tag, 4비트 index
- 오예측 시: index = `PC[5:2]`에 현재 PC, target, predictor 기록, tag = `PC[17:6]`

**JPU 로직**
```c
if (BTB_V && BTB_TAR == R_TAR) {
    if (R_J) BTB_PRED++;
    else     BTB_PRED--;

    if (BTB_J != R_J)
        FLUSH();

    BTB_ST();
} else {
    if (R_J) {
        BTB_OVERRIDE();
        FLUSH();
    } else {
        if (BTB_J)
            FLUSH();
        BTB_PRED--;
    }

    BTB_ST();
}
```

### flush & rollback
CPU의 추측이 틀렸을 때 발생. store-buffer는 commit 이후에만 쓰므로 flush 로직 불필요.

- [x] **wrong-speculation 상태** — id-dispatcher에 추가
  - com1 명령에 wrong-spec 플래그가 있으면: com2 비활성화, 현재 inst-id로 flush & rollback 호출 (파라미터 id보다 **큰** 것을 비교)
  - ROB는 별도 비교/삭제 회로 불필요 — id-generator가 되돌아가면 commit 전에 올바른 데이터로 덮어씀
- [x] **flush 메커니즘**
  - fetch/decode/issue valid-buffer 리셋
  - 호출된 명령 이후의 모든 명령 취소 (비교기 다수 필요)
- [x] **rollback**
  - architectural 상태 복원
  - speculative 상태 버퍼를 architectural 상태로 채움
- 타이밍
  - rollback과 flush는 동시에 발생
  - fetch valid-buffer는 PC 준비 시점(rollback & flush 1사이클 후)에 켜짐
  - valid-buffer 자체 on 신호 버퍼가 1사이클 지연을 더할 수 있음 → 그 경우 rollback/flush로 직접 활성화

## 커밋 히스토리 요약 (dev 브랜치, 오래된 순)

| 구간 | 커밋 | 내용 |
|---|---|---|
| 초기 단계 | 1809f1c ~ dcf7cad | 초기 커밋, flush 시스템 최초 구현, 파일 구조 정리, FU/store-arbiter/id-manager에 flush 로직 |
| 파이프라인 기본 구축 | fc528fd ~ b387b50 | LDB clear 유틸, branch 로직 |
| CPU 2차 반복 | 238bc55 ~ 5323b13 | cpu no2 부분 동작, 메모리/스택/id-generator/jmp 버그 수정 |
| 문서 | 3668f89 | 초기 readme |
| call/ret 전면 개편 | 689a6ef ~ 3368760 | JPU 오분기 수정, push 추가, stack flush 메커니즘, ret 오류 수정 |
| CPU 동작 확인 | 34e0e76 ~ f2d33a2 | cpu 동작 시작, branch commit / phantom store 수정, forwarding 이슈 발견 |
| block / 레지스터 | ccd0a41 ~ 4f2865a | block 테스트, add_terminal.txt 검증, 레지스터 슬라이싱 수정 |
| 대형 안정화 | 48bdd24 ~ 59e713a | ALU / 메모리 flush 수정 ("아직 문제 많음") |
| queue → age matrix | 69c485a ~ 24df657 | load/store buffer ordering을 age matrix로 전환 |
| 문서화 | 8afa9ee | CLAUDE.md / CLAUDE.ko.md |
| 빌드 / 파서 | 6b8b02b ~ 734c6a4 | 메모리 동작 확인, gcc/msvc 차이, 파서 closure 비교자·hash-order 버그 |
| 반복 버그 수정 | 5c33449 ~ da2d07e | 랜덤 로딩 버그, stall 문제 일부 수정 |
| 정리 | 32db929, f628a96 | 테스트 / 누락 커밋 |
| CPU 수정 완료 | 80dae21 ~ 5b7e6ba | "cpu finally fixed!", cpu-32 업데이트 |
| block/halt, call-with-base, 인터럽트 | a2fee40 ~ 858bfda | block/halt 추가, call base 겸용, 인터럽트 구현 시작 |
| 부동소수점 / 툴 재구성 | f5c62b6 | floating point 라이브러리 추가, tools 구조 정리 |
| headless sim 버그 수정 | ed2b121 | headless sim으로 찾은 cpu-32 flush/rename 버그 수정, calc / dsim 추가 |
| test RAM/ROM, boot 이미지, picalc | 4a48323 | 외부 RAM/ROM 테스트 회로, boot 이미지, picalc, todo.md |
| boot / 바이트 주소 | 9af475e | bootloader, ROM·BTB 수정, 바이트 문자열 전환, 에뮬레이터 이동 |
