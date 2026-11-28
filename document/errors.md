# 회로 에러 기록

INT-MMU / buff-outstanding 작업(2026-10-29 ~ 11-07) 중 dsim으로 찾은 회로 에러를 모아 둔 문서입니다. 비슷한 증상이 다시 나오면 여기서 먼저 찾아봅니다.

> - 좌표(`Register@1460,2520` 등)와 half 번호는 당시 회로 기준입니다. 회로가 바뀌면 달라집니다.
> - 상태: ✅ 고쳐짐(테스트 통과로 확인) · ⚠️ 확인 필요 · ❌ 남아 있음
> - 재현/확인 방법: `bash tools/dsim/runtests.sh [-j N] [-nobuild] [이름...]` (출력은 `program/build/dsim/`). dump는 `tools/dsim/README.md`의 `-Dtap`/`-Ddelta`/`-Dprobe`.

## 요약

| # | 날짜 | 영역 | 증상 | 상태 |
|---|---|---|---|---|
| 1 | 10-29 | MMU | boot의 첫 load 2개 결과가 안 옴 | ✅ |
| 2 | 10-29 | MMU (클럭) | selftest `Q`에서 멈춤, store buffer busy 고정 | ✅ |
| 3 | 10-30 | 메모리 응답 | flush 뒤 다시 실행한 `ret`이 0을 받음 | ✅ |
| 4 | 11-02 | load-buffer mid | boot 복사 루프가 1~2회만 돎 | ✅ |
| 5 | 11-02 | 메모리 mid 반환 | 응답 mid가 항상 0 | ✅ |
| 6 | 11-02 | id-manager self flush | memtest `a`에서 commit이 멈춤 | ✅ |
| 7 | 11-02 | ROB-SC | selftest가 처음부터 다시 시작 | ✅ |
| 8 | 11-03 | inst-wfile self flush | memtest `c`에서 load가 자기 자신을 기다림 | ✅ (self flush 구조 변경) |
| 9 | 11-04 | id-manager valid/commit | boot에서 commit 포인터가 id를 건너뜀 | ✅ |
| 10 | 11-04 | load-buffer `osf` | load-buffer가 차서 LDB busy | ✅ |
| 11 | 11-04 | self flush된 load commit | 이전 값으로 commit될 수 있음 | ✅ (11-07, regFile에 안 써서 값이 안 보임) |
| 12 | 11-04 | load-buffer OLDD | memtest2 `a7`이 `7070` (정답 `7171`) | ✅ |
| 13 | 11-04 | load-buffer 응답 매칭 | picalc `pop`이 다른 엔트리 값을 받음 | ✅ |
| 14 | 11-04 | 버스 mid | store 응답이 load 데이터로 들어감 | ✅ (store 응답 끔) |
| 15 | 11-04 | 프론트엔드 flush | calc `ret`이 0x40a00000으로 점프, picalc 끝자리 틀림 | ✅ (11-05 최종 수정, flushstall 통과) |
| 16 | 11-02 | res-station.dig (non-debug) | `inst-wfile` 핀 연결이 밀림 | ❌ |
| 17 | 11-05 | load-buffer forward 폭 | `st .8L` 직후 `ld .32B`가 `000000AA` (정답 `112233AA`) | ✅ (11-27, 55번) |
| 18 | 11-07 | `st.stack` base | issue가 밀린 `st.stack`이 더 젊은 pop/ret/push가 바꾼 stack을 주소로 씀 | ✅ (11-07, `st.stack`이 PCA 사용) |
| 19 | 11-16 | TRAP `iret` load id | `iret`의 load가 돌아오지 않음 (`Ii` 뒤 stall) | ✅ |
| 20 | 11-16 | `iret` commit ECLR | `iret`이 0x178이 아니라 자기 PC+4(0x1c)로 감 | ✅ |
| 21 | 11-16 | BTB 학습 | 인터럽트 없이 fetch가 핸들러로 들어감 | ✅ |
| 22 | 11-16 | `int-level` / `int-tmplv` 쓰기 | `iret` 뒤 `int-level`이 복원되지 않음 | ✅ |
| 23 | 11-17 | IPU `INT_req` 응답 | `syscall` 뒤 `L00000278 A`에서 stall | ✅ |
| 24 | 11-18 | 수락 RS_FF S=R=1 | `syscall`이 seed에 따라 약 1/4 확률로 stall | ✅ |
| 25 | 11-18 | fetch hold (wait가 clr보다 먼저) | `syscall` 반환 PC가 다음 명령 +4 (한 명령 건너뜀) | ✅ (11-18, 14a PASS) |
| 26 | 11-18 | 공용 stack 가산기 mux sel = clr | flush와 겹친 `iret` 뒤 istack·stack이 4 어긋남 | ✅ (11-18, 14d PASS) |
| 27 | 11-18 | 진입 때 `int-level` | `syscall` 핸들러의 `int-level`이 0x10 (`int_pri[1]` = 1인데) | ✅ (11-19, 14b PASS. ker는 20b로 남음) |
| 28 | 11-18 | 진입 때 `int-tmplv` | 진입이 `int-level`을 `int-tmplv`에 복사하지 않음 | ✅ (11-19, 16b PASS) |
| 29 | 11-18 | `syscall` 직전 IRQ | `INT_req` 2~14 half 전의 IRQ 펄스가 사라짐 | ✅ (11-19, 장치 latch 규칙으로 15b PASS, 35번) |
| 30 | 11-18 | 연속 `syscall`의 `iret` | 두 번째 `iret`이 첫 반환 PC로 간 뒤 stall | ✅ (11-19, 19a~c PASS) |
| 31 | 11-18 | `ld ; ld ; iret` | `iret`이 돌아오지 않음 (타이밍 의존) | ✅ (11-19, 22 PASS) |
| 32 | 11-19 | `syscall` 수락 잠금 | `syscall`이 한 번 더 진입해 vector 0으로 가고 처음부터 다시 시작 | ✅ |
| 33 | 11-19 | `iret` 완료 비트 (RET_INT, V1) | `iret` 뒤 commit이 멈춰 RS_full로 stall | ✅ |
| 34 | 11-19 | istack 갱신 register 지연 | `iret` 뒤 istack이 +4로 돌아오지 않음 | ✅ |
| 35 | 11-19 | IPU 출력 register 덮어쓰기 | 내보내던 IRQ를 `syscall` `INT_req`가 덮어 IRQ가 사라짐, `syscall` acp가 T_num 선으로 감 | ✅ 장치 규칙으로 회피 / ⚠️ acp 선 |
| 36 | 11-19 | FT RS_FF S=R=1 | `syscall` wait와 IRQ `run_int`가 같은 클럭이면 fetch가 멈추지 않아 반환 PC 틀림 (15c) | ✅ |
| 37 | 11-19 | IRQ 수락 latch 취소 | 수락 뒤 `stexp`로 level을 올려도 진입 (16b, R = 0x1dc..) | ✅ |
| 38 | 11-19 | IPU ICLR 창의 펄스 | acp 직후 3클럭 안의 펄스가 사라짐 (17b) | ✅ 장치 규칙 (acp까지 high) |
| 54 | 11-27 | ROB-SC self flush 복구 | self flush 뒤 stack(SCS)이 다른 entry 값으로 복구돼 처음부터 다시 시작 (ststld) | ✅ |
| 55 | 11-27 | store-buffer forward 폭 | 부분 store 뒤 더 넓은 load가 store 값만 받음 (17번) | ✅ |
| 56 | 11-27 | store-buffer 다음 word 비교 | 정렬 load가 다음 word store에서 forward (ldstmix m4) | ✅ |
| 57 | 11-27 | store-buffer forward byte offset | `st.32B X` 뒤 `ld.8L X+1`이 byte 0을 받음, 고친 뒤 offset 1과 3이 뒤바뀜 (straddle) | ✅ |
| 58 | 11-27 | alignment fault (id 5) | byte 접근이 fault, word/half 비정렬은 통과; fault id가 MMU 단 명령 | ✅ |
| 59 | 11-27 | `pop flag` 뒤 ALU | 바로 뒤 ALU가 pop한 flag를 못 보고, 그 ALU의 flag 결과도 사라짐 | ✅ (11-28) |
| 60 | 11-27 | hex 출력 루프 deadlock | `shiftr` reg + `cmpi`/`subi`/`jmp` 루프가 PC=0x214에서 멈춤 (carry-loopshow) | ❌ |
| — | 10-17 | 기존 알려진 문제 | `halt`가 분기보다 먼저 실행 | ✅ 재현 안 됨 (11-07 `haltbr`), CLAUDE.md에서 삭제 |

2026-11-07 기준 runtests 13개 중 `knownbug`(k1~k3, 17번)만 DIFF이고 나머지는 모두 에뮬레이터와 같습니다. `selfflush`(a~l), `haltbr`(H1~H10)도 모두 같습니다.

회귀 테스트(`program/test/`, 설명은 `program/document/test.md`):
- `flushstall`: 15번 (store K개 + nop N개 뒤 call/ret/jmp/분기 flush) → 모두 PASS (11-05 최종 수정 후)
- `ldstmix`: 13·14번 (load/store 섞기, 연속 load, pointer chase) → 모두 PASS. store 응답을 다시 켜면 재확인
- `knownbug`: 17번, 분기 직후 halt → DIFF가 정상 (k1~k3 FAIL, halt 문제는 이번엔 재현 안 됨)
- `selfflush`: 11번, 18번 (self flush되는 load 패턴, `st.stack` 뒤 push/pop/ret) → 모두 PASS (11-07, runtests에는 아직 없음)
- `haltbr`: halt 문제 (늦게 풀리는 taken 분기 바로 뒤 halt, H1~H10은 `#define H<n> 1`로 이미지마다) → 모두 PASS (11-07, runtests에는 아직 없음)

---

## 1. MMU_INST가 한 칸 뒤 id를 잡음 (10-29)
- **증상:** INT-MMU에서 selftest가 아무것도 출력하지 않습니다. boot의 load 3개 중 앞의 2개(시작 주소, 길이)는 결과가 안 오고, `gen[3]`(길이)가 비어 복사 루프를 건너뜁니다.
- **원인:** MMU 박스의 `MMU_INST` 레지스터가 `6, 9, f`를 잡는데, 실제 MMU로 들어간 load id(`MMU_inst`)는 `5, 8, d`였습니다. enable이나 입력 타이밍이 한 칸 어긋났습니다.
- **참고:** 이름 변경 전에는 MMU 박스와 DIR 회로가 `DIR_LD`/`DIR_CLR` 넷을 같이 써서 넷마다 출력이 둘이었습니다(`MMU_LD`/`MMU_CLR`/`MMU_INST`로 분리).

## 2. MMU_LD가 반클럭 펄스가 됨 — C / C_ct 혼용 (10-29)
- **증상:** selftest가 `P`까지 나오고 `BEGIN('Q')` 직후 멈춤. `STB_busy`, `halt-FT-DC-ISS`가 1로 고정.
- **원인:**
  - `MMU_acp = MMU_req`(MMU-BL은 바로 받음)이고 `MB_free = acp | MMU_CLR | idle`, `MMU_LD = new & MB_free`.
  - `MMU_LD`는 `C_ct` 레지스터(`MMU_INST` 등)와 `C` 레지스터(`MMU_virt` 등) 양쪽의 enable입니다.
  - `req`가 `C` 에지에 1이 되면 같은 반클럭 안에 `MMU_LD`가 1이 됐다가 다음 `C_ct` 에지에 0이 됩니다. `MMU_INST`는 새 id(9)를 잡고 `MMU_virt`는 옛 주소(push의 `1e70`)를 그대로 둡니다.
  - 'Q' store는 자기 주소 변환을 받지 못해 store buffer가 계속 기다립니다.
- **고친 것:** `acp`를 `C_ct`로 래치해서 `MMU_LD`가 한 클럭 유지되게 함. 이후 `bf78b59`에서 CPU 전체를 한 클럭 에지로 바꿔 구조적으로 사라짐.
- **교훈:** 한 enable이 서로 다른 클럭의 레지스터를 같이 잡으면 그 enable은 한 클럭 이상 유지돼야 합니다.

## 3. D_accept가 상수 1이라 load 결과 버퍼가 계속 다시 잡음 (10-30)
- **증상:** selftest `T`(첫 `fp_add`)에서 멈추거나 처음부터 다시 실행. 분기 flush 뒤 다시 실행한 `ret`(같은 id 8)이 메모리 응답 전에 데이터 0을 받아 0으로 점프.
- **원인:**
  - 결과 버퍼 capture enable `And@1260,2940 = D_accept & ~Q(RS_FF@940,2940) & !MEM_TYPE-_ST`.
  - `D_accept`가 기본값 1인 In 핀에만 연결돼 있어서, 마지막 접근이 load면 enable이 계속 1입니다.
  - 데이터 레지스터가 매 클럭 버스를 다시 잡아(`e7c` 다음 `0`), valid `RS_FF@1460,2960`은 S가 1로 고정돼 reset되지 않습니다. flush 뒤에도 "id 8, 데이터 0, 준비됨"이 남습니다.
- **고친 것:** `D_accept`를 메모리 쪽 `Or@-1920,-580`에서 구동(펄스가 됨).
- **남은 점:** 펄스가 2클럭인 경우가 12번 있었습니다. 메모리가 2클럭 걸리면 두 번째 클럭에도 버스가 유효한지 확인해야 합니다.

## 4. load-buffer 엔트리 mid를 저장하는 시점이 틀림 (11-02)
- **증상:** boot 복사 루프가 1~2회만 돌고 PC=0으로 점프, 빈 RAM을 실행(`PC=27100`), 출력 없음.
- **원인:** `load-buffer-debug.dig`의 `M*_mid` 레지스터 enable이
  - 처음엔 `M*_DE`(데이터 쓰기), 다음엔 `M*_RS`(엔트리 비울 때)에 연결돼 있었습니다.
  - 그래서 기다리는 동안 mid가 0(또는 옛 값)이고, 버스로 `D_MID=0`이 나가며, 응답 mid 0에 아직 주소도 없는 엔트리까지 같이 일치합니다.
- **고친 것:** mid 레지스터 en과 `XOr In_2`를 `And@1180,-300`(`M1_RS & M1_AE`, 주소 쓰기 시점) 출력에 연결(M2~M4 동일).

## 5. 메모리가 mid를 돌려주지 않음 (11-02)
- **증상:** 첫 load가 `D_MID=1`로 나갔는데 응답 `D_AMID`가 0. 주소도 없는 M3이 데이터를 받고 M4는 영원히 기다림(`PC=ffffff48` stall).
- **원인:** `D_amid` 넷이 `In'D_AMID'`와 `Register@960,2940.D`에만 연결. 메모리 쪽에서 `D_MID`를 `D_AMID`로 되돌리는 연결이 없었습니다.
- **고친 것:** `D_mid`와 `D_amid`를 같은 넷으로 연결.
- **함께 나온 것:** 이 무렵 `D_accept` 넷이 In 핀과 메모리 쪽 `Or`에 이중 구동돼 클럭이 half 42에서 멈췄습니다. `D_answer`/`I_answer`로 분리해 해결.

## 6. self flush 때 id-manager가 다음 id를 clr_id+1로 되돌림 (11-02)
- **증상:** memtest `a`에서 멈춤. load가 앞선 store와 순서 위반 → self flush(`clr_self=1`, `clr_id=c`) 뒤 commit이 c에 머묾, RS가 차서 frontend가 멈춤.
- **원인:** `sclr`가 지울 범위(`clrs_16`)에만 쓰이고 다음 id 계산에는 안 들어감. 다시 가져온 load가 d를 받아 c는 다시 생기지 않습니다.
- **고친 것:** self flush면 다음 id를 `clr_id`로.

## 7. ROB-SC stack 복구 값이 반클럭 늦음 (11-02)
- **증상:** selftest가 `A ? ABCD1234` 뒤 처음부터 다시 시작.
- **원인:** 오래된 `jmp.neg`가 flush할 때 regFile이 stack 복구 값(`CT_SC-_FD`)을 받는 그 에지에 `FD`가 아직 옛 값(`1e74`)입니다. `ROB-SC`의 `roll_id`/`none`이 flush가 반영된 레지스터로 계산돼 반클럭 늦게 맞는 값(`1e70`)이 됩니다. 다시 실행한 `ret`이 `1e74`에서 0을 읽음.
- **고친 것:** ROB-SC 수정(11-03 00:34 저장).

## 8. self flush 때 load가 자기 자신에게 의존 (11-03)
- **증상:** memtest `c`에서 stall(`com1_inst=4`).
- **원인:**
  - clear 때 flush된 writer를 가진 레지스터는 `rcom_id = {0, clr_ep, clr_id}`(`Splitter@2720,880`)가 됩니다.
  - 일반 분기 flush는 `clr_id`가 분기 자신이라 다시 실행되지 않지만, self flush는 `clr_id`가 다시 실행될 load 자신입니다.
  - base 레지스터(gen13)의 writer가 flush되면 다시 dispatch된 load가 gen13 의존성을 id 4(자기 자신)로 받아 영원히 ready가 안 됩니다.
  - `a`/`b`는 `ld.zero`라서 통과, `c`부터 base 레지스터를 써서 걸림.
- **해결 방향(채택):** self flush를 "load id에서 일어나는 일반 flush + load만 replay"로 바꿈. id는 load 다음으로, fetch는 load PC+4부터, load-buffer 엔트리는 남기고 데이터만 무효화 후 다시 읽기.
- **처음 의심한 것(아님):** 처음엔 flush 뒤 frontend latch의 다음 반복 명령이 되돌린 id로 dispatch된다고 봤지만, dump로 frontend는 clr에 정상 반응함을 확인했습니다.

## 9. id-manager valid 마스크가 한 사이클 늦고, commit 증가량이 valid2로 정해짐 (11-04)
- **증상:** memtest가 boot(`PC=ffffff3c`, half 2140)에서 멈춤. `com1_inst`가 3에서 5로 가며 4를 건너뜀.
- **원인:**
  1. valid D에 `used` 레지스터의 Q(`US`)를 써서 clr된 id의 valid가 다음 사이클까지 남음. (HEAD는 `cmks_16 = NOr(com1_16, com2_16, …, clr_16)`으로 바로 지웠음)
  2. `Add@1640,480.b = {valid1 & !valid2, valid2}`라서 com2가 0이어도 남은 valid2 때문에 +2.
- **고친 것:** 마스크를 `used`의 다음 상태(또는 `cmks_16`)로, 증가량을 `{com1 & !com2, com2}`로.

## 10. load-buffer `osf`가 응답 없는 mid에 남음 (11-04)
- **증상:** memtest `c`에서 LDB `full=1` → busy. 빈 엔트리 2개(M2 mid 3, M4 mid 2)가 `osf=1`인 채로 막힘.
- **원인:**
  - M4: self flush 순간 LDB가 `W_MMU`/`mid_WP=2`로 `osf`를 켰는데, MB는 그 load를 버리고 store를 처리. 응답이 오지 않음. MB가 버린 요청은 `osf`를 켜면 안 됩니다.
  - M2: 응답(8550)이 `osf`가 켜지기(8554) 전에 도착해 다시는 지워지지 않음.
- **고친 것:** load-buffer 수정(11-04 23:26 저장) 뒤 memtest 통과.

## 11. self flush된 load가 이전 값으로 commit될 수 있음 (11-04) ✅ 값은 보이지 않음 (11-07)
- **관찰:** self flush 뒤에도 load id의 완료 비트(`V1_inst`)가 남아 앞 store와 함께 commit됨(com2). 엔트리도 비워져 replay되지 않음.
- 같은 dump에서 "ROB head id 5가 `DV=1`인데 완료가 안 됨"도 보였으나 원인은 안 봤습니다(이후 재현 안 됨).
- **11-07 확인 (memtest `a`, half 8934~8960, zseed=1):** 실제로 생깁니다.
  - LDB가 load id c에 옛 값 0을 씀 → 8940 self flush(`clr_id`=c, `FL_MEM_ID`=c, `MEM_FPC`=0x50 = load 자신의 PC) → 8944 com2로 c가 store b와 함께 commit.
  - fetch가 0x50부터 다시 시작해 load가 **새 id d**로 다시 실행 → 8958 d가 0x110(정답)을 받고 8960 commit.
  - 지금 self flush는 "load id replay"가 아니라 "load PC부터 다시 fetch"입니다. 옛 id c는 commit되지만 regFile에 쓰지 않습니다(probe R2는 c commit 때 그대로, d commit 때 0x110).
  - dest = base인 경우(`selfflush` d)도 c commit 때 base가 유지되어, 다시 실행된 load가 맞는 base를 씁니다.
- **테스트:** `program/test/memory/selfflush.asm` a, b, c, d, e, h, i, k (16라운드, zseed 1~3) 모두 에뮬레이터와 같음.
- **확인 안 한 것:** 옛 id c commit과 다시 fetch된 d 사이에 인터럽트가 들어오는 경우(정밀 상태에서 load를 건너뛸 수 있음), IO처럼 읽으면 상태가 바뀌는 주소의 load(두 번 읽힘).
- **고칠 방향(필요하면):** self flush 때 load id 완료 비트 지우기. 지금은 결과에 영향이 없음.

## 12. flush 직후 OLDD 플래그가 다시 켜져 데이터 없이 출력 (11-04)
- **증상:** memtest2 `a7`이 `00007070`(정답 `00007171`).
- **원인:**
  - 출력 조건 `And@420,2240 = !(RS | flush) & OLDD & VO`에 `DV`가 없음.
  - 순서 플래그(`RS_FF@2160,2440` 등, 엔트리 자기 bit)의 S 입력이 `Or@940,1780 = 출력 벡터 | D_FF@780,1820.Q`(한 클럭 늦은 flush)라서, flush가 R로 끈 다음 클럭에 다시 켜짐.
  - replay load가 그 엔트리를 받자마자 메모리 요청 전에 남은 `7070`을 내보냄.
- **고친 것:** `D_FF@780,1820` 제거.
- **더 안전하게:** 엔트리 할당 때 플래그 reset, 출력 조건에 `DV` AND.

## 13. load-buffer가 mid만 보고 응답을 받음 (11-04)
- **증상:** picalc `0: 1415` 뒤가 틀림. 0x5ec `pop gen[5]`가 8 대신 `0x14`(옆 엔트리 값)를 받음.
- **원인:** 응답 쓰기 조건이 `M_mid == D_MID`뿐. mid 카운터는 6bit라 64번마다 돌아오고, 엔트리를 비울 때 mid를 지우지 않음. 주소도 없는 M1(옛 mid `26`)이 M3의 응답을 같이 받음.
- **고친 것:** 응답 조건에 `M_osf`(요청 내고 기다리는 중) 추가.
- **참고:** 저장 중 `M1_FWD` 터널이 안 연결된 상태면 NetQuery가 `A tunnel M1_FWD is not connected!`로 실패합니다.

## 14. 버스 mid 레지스터가 LDB_MID만 받음 — store 응답에 load mid (11-04)
- **증상:** picalc `0: 0015` 뒤 배너가 다시 나오고 이상한 PC(`37661`). 0x44c `pop gen[5]`가 1 대신 0.
- **원인:**
  - 버스 컨트롤러(cpu-test x≈760~1600, y≈2300~2850)는 `MEM_LD`에서 STB/LDB 중 이긴 쪽의 종류·주소·모드를 mux로 고르는데, **mid `Register@1460,2520`의 D는 `LDB_MID`에 바로 연결**(mux 없음). store-buffer에는 mid 출력이 없습니다.
  - store가 이겨도 `Driver@1580,2540`이 `D_MID`에 대기 중인 load의 mid(8)를 냄. load-buffer가 그 store 응답(`D_ST`)을 load 데이터로 받음.
- **처음 잘못 본 것:** "MMU가 요청별 mid를 안 남긴다"고 했으나 틀림. `MMU-D.dig`는 조합 회로(`acp = req_c`, `mid_o = mid_c`)라 정상입니다.
- **고친 것:** store일 때 응답을 끔. (다른 방법: load-buffer 응답 조건에 `D_LD` AND, 또는 mid 레지스터 앞에 `MEM_TYPE_ST` mux)

## 15. 버스 stall 중 flush가 오면 FT 단의 옛 fetch가 살아남음 (11-04)
- **증상:** calc `2*3-1 = ` 뒤 `PC=40a00050`에서 멈춤(0x11ac `ret`이 0x40a00000=5.0f로 점프). picalc `10 10`의 끝자리 `…845`(정답 `…846`).
- **dump (calc_nh):**

  | half | 내용 |
  |---|---|
  | 490068 | FT 단에 f14(`PCS_FT=f14`). 버스가 store로 바빠 `CT_BUS_IAR=0`, `dc_e=0` |
  | 490070 | f0c `call`이 `FL_JPU`/`clr=1`, PC→45c. DC/ISS valid는 지워지지만 FT 레지스터에 f14가 남음 |
  | 490080 | stall이 풀리며 DC가 f14를 유효한 명령으로 받음 |
  | 490086 | f14(`ijmp.neg zero, 0xf98`)가 실행돼 f98로 flush, 잘못된 경로 실행 |

  이후 stack이 4 작아져(0x1da8, 정답 0x1dac) `ret`이 0x1dc8을 읽음.
- **원인:** `Register'DC'.D = FT & !(clr|wait)`인데 `FT`(`RS_FF@-140,-240`)는 명령별 valid가 아니라 "실행 중" 플래그. stall 없이 flush하면 같은 사이클에 FT 단이 새 PC로 바뀌어 안 드러났음.
- **처음 의심한 것(아님):** `ret`이 이전 `stack`을 씀 / load-buffer 데이터 섞임.
- **고친 것:** 사용자가 프론트엔드 수정(11-05). calc, picalc 모두 통과 → picalc 끝자리도 같은 원인이었던 것으로 봄.
- **1차 수정 뒤 남은 것 (`program/test/flushstall.asm`):** `c10`, `c30`~`c33`이 `FAIL 00050100`(call 뒤 독 명령 하나 실행).
  - 1차 수정은 FT 단 valid를 따로 두는 방식이었습니다. flush 뒤 첫 FT 사이클은 `I_A_R`의 낡은 주소를 fetch하고 DC=0으로 버리는 "버리는 사이클"입니다.
  - 그런데 DC가 `adv`만 보고 열려서, 그 사이클에 store 때문에 fetch가 거절되면(`I_responce=0`, `CT_BUS_IAR=0`) `I_A_R`이 갱신되지 않은 채 DC가 낡은 주소의 명령을 받았습니다.
- **2차: clr 다음 클럭에 `CT_BUS_IAR`을 강제로 켬** (`D_FF@-200,0` → `Driver@-80,20`). `I_A_R`/`PCS_FT`에 새 PC가 들어갑니다.
  - `CT_RF_PCSE`가 같이 켜지지 않아 PC = `I_A_R`이 됐고, 첫 주소를 두 번 fetch했습니다(callee 두 번 실행, `FAIL 00040200`).
- **3차: 강제 사이클에 `CT_RF_PCSE`도 켬.** PC = `I_A_R`+4가 유지됩니다(BTB는 PC 증가 때 자동 계산).
  - 이제 `I_A_R`이 맞으니 버리는 사이클이 진짜 첫 명령을 버렸습니다(selftest `A ? 00000000`, 0x840 `set gen[18], 0xF`가 빠짐).
- **최종: DC enable에 강제 IAR 신호를 OR** (`adv | kill | CLRD`). 강제 사이클에 DC가 1이 되어 버리는 사이클이 없어집니다.
  - 11-05 runtests 13개가 모두 SAME입니다(knownbug는 예상대로 DIFF).
- **교훈:** fetch 주소 레지스터(`I_A_R`)와 PC는 항상 같이 움직여야 합니다(`CT_BUS_IAR`과 `CT_RF_PCSE`). DC valid는 "DC가 받을 사이클의 `I_A_R`이 유효한가"와 맞아야 합니다.
- **찾는 방법:** 에뮬레이터와 stack store 순서를 비교해 처음 갈라지는 store를 찾음(f0c store 다음, 에뮬레이터 0x468 / 회로 0xea0).

## 16. res-station.dig (non-debug) 핀 연결이 밀림 (11-02) ❌
- `inst-wfile`에 `clr_self` 입력이 생기면서 핀 위치가 밀렸는데 `res-station.dig`(cpu-32.dig가 씀)는 갱신 안 됨: `clr_id` 연결 없음, `clr_ep`←`clr_id`, `clr_self`←`clr_ep`.
- `res-station-debug.dig`처럼 손으로 맞춰야 합니다(`sync-nondebug.sh`는 2026-11-19에 삭제). (self flush 구조가 바뀌어 `clr_self`가 없어졌다면 다시 확인)

## 17. load-buffer forward가 접근 폭을 보지 않음 (11-05) ✅ (11-27, 55번)
- **증상:** `knownbug`에서 `0x11223344`를 쓰고 부분 store 직후 같은 주소를 `ld .32B`로 읽으면 틀립니다.

  | 케이스 | store | 읽은 값 | 정답 |
  |---|---|---|---|
  | k1 | `st .8L` 0xAA | `000000AA` | `112233AA` |
  | k2 | `st .16L` 0xBEEF | `0000BEEF` | `1122BEEF` |
  | k3 | `st .8H` 0xCC00 | `000000CC` | `112233CC` |

- **원인:** `LDB_forward`가 store의 폭을 보지 않고, 0으로 확장된 store 데이터를 더 넓은 load에 그대로 넘깁니다. 메모리의 나머지 바이트가 합쳐지지 않습니다.
- **메모리는 맞음 (11-06 확인):** store와 load 사이에 300번 도는 루프를 넣으면 k1~k3 모두 PASS이고 에뮬레이터와 같습니다(`zseed=1`, 한 번 실행). 버스의 `D_MD`도 k1=2, k2=1로 맞습니다.
- **처음 잘못 본 것:** "store `D_MD`가 항상 0이라 4바이트를 쓴다". 10-18에 고친 옛 버그의 설명이 CLAUDE.md에 남아 있었고, 11-05 결과가 그 증상과 비슷해 같은 원인으로 봤습니다.
- **고칠 방향:** store가 load 범위를 다 덮을 때만 forward하고, 아니면 store가 끝날 때까지 기다린 뒤 메모리에서 읽습니다. 또는 바이트 단위로 합칩니다.
- **확인 안 한 것:** 같은 폭이나 더 좁은 load(`st .32B` 뒤 `ld .8L` 등)의 forward, 주소가 어긋난 경우(`st .8L` at +1 뒤 word load).

## 18. `st.stack` base가 더 젊은 pop/ret/push의 stack을 씀 (11-07) ✅
- **증상 (`selfflush`, zseed 1~3 모두 16/16 FAIL):**
  - `g`: call 뒤 callee에서 늦은 data로 `st.stack gen2, 0`(반환 주소 덮어쓰기) ; `ret` → 옛 주소로 돌아감.
  - `j`: `push zero` ; 늦은 data `st.stack gen2, 0` ; `pop gen1` → gen1이 옛 값 0.
  - `l`: `set stack, stk_end-8` ; 늦은 data `st.stack gen2, 0` ; `push gen10` ; 루프 ; `ld.zero stk_end-8` → 옛 값. load가 근처에 없어도 생김.
- **원인:** load 순서가 아니라 **store 주소**가 틀림. data가 늦어 issue가 밀린 `st.stack`이 dispatch 때가 아니라 issue 때(더 젊은 stack 변경이 반영된) stack을 읽음.
  - store buffer 주소: g 0x8f8(정답 0x8f4, ret 뒤), j 0x9b8(정답 0x9b4, pop 뒤), l 0xa24(정답 0xa28, push 뒤).
  - 주소가 틀리니 self flush도 나지 않음.
  - g dump: 20508 LDB가 ret(id 8)에 옛 반환 주소 0x2ac → 20512 JPU가 0x2ac로 점프 → 20538에야 store(id 7) STB A=0x8f8.
- **고친 것:** 사용자가 `st.stack`이 PCA를 쓰도록 수정(`regFile.dig` 포함, 11-07). `selfflush` a~l 모두 PASS, runtests 13개는 knownbug만 DIFF(예전과 같음), `haltbr` 모두 같음.
- **확인 안 한 것:** issue가 밀린 `ld.stack`의 base, stack 외 SC 레지스터.

## 19~24. 인터럽트 (`iret`, `syscall`) (11-16 ~ 11-18) ✅
재현은 `python tools/dsim/inttest.py`(설명은 `program/document/test.md` "인터럽트 회귀 테스트"). 19~22는 `int_iret`(7a~7c), 23~24는 `int_sys`(8a, 8b). 모두 사용자가 고쳤고 11-18에 PASS 20, SKIP 1(11번)입니다.

- **19. `iret` load의 id/epoch:** `iret`은 issue 때 `CT_ISS-_OLDB`로 자기 id(`inst_IS`)의 load-buffer 칸을 받습니다. 그런데 TRAP_IRET(`Register@1100,3980`)이 내는 load 주소 쪽 id/epoch가 `int-inst`였습니다. load 응답이 그 칸과 맞지 않아 `iret`이 돌아오지 않았습니다. → `trap-inst`로 바꿈. load가 0x178을 받습니다.
- **20. `iret` commit의 ECLR:** `iret` commit 때 `Fl_ECLR`가 `iret` PC+4(0x1c)로 redirect해서, JPU가 낸 0x178 redirect를 덮었습니다. → 사용자가 고침(`iret`이 0x178로 돌아감).
- **21. 인터럽트 진입과 `iret`이 BTB에 씀:** 진입과 `iret`에서 `JPU_EN`이 BTB 쓰기를 켜서 0x174 → 8을 배웠습니다. 그 뒤 인터럽트가 없어도 fetch가 핸들러로 들어갔습니다. → 두 경우 BTB 쓰기를 뺌.
- **22. `int-level` 복원:** `level` 레지스터(`Register@-540,3600`, en = `CT_ILVE`)를 TRAP_IRET이 켜는 driver가 없었습니다. 또 `Driver@320,3380`(RX_ILVTE)가 `CT_ILVTE`가 아니라 `CT_ILVE`를 몰았고, `CT_ILVTE`에는 PullDown이 없었습니다. → 고침. 7b, 7c PASS.
- **23. IPU가 `INT_req`에 응답하지 않음:** `IPU-test.dig`의 `INT_req` 입력이 같은 이름 tunnel에만 이어져 있었고, `INT_rqac`는 Ground였습니다. → IPU가 `INT_req`를 `Register@1780,220`에 받아 `INT_rqac`로 돌려주고, 같은 enable로 `INT_id-_bus`가 가리키는 칸의 vec/pri를 `INT_vec`/`INT_lev`에 잡습니다. `syscall`은 id 1입니다(11-18, id 0은 panic/비상용). 잡힌 level이 0이면 `INT_out`이 오르지 않으므로 그 칸에 `int_pri` ≥ 1이 있어야 합니다.
- **24. 수락 RS_FF의 S=R=1:** `syscall`에서 `int-forced`가 `run_int`를 올리는 클럭에 `INT_MASKED`가 아직 1입니다(같은 에지에 풀림). 그래서 `RS_FF@-240,2860`(S = `run_int`, R = `INT_acp | INT_MASKED`)과 `RS_FF@-480,2780`(S = `run_int`, R = `INT_MASKED | …`)이 둘 다 S=R=1을 받습니다. Digital의 RS_FF는 S=R=1이면 `Math.random() < 0.5`로 출력을 정합니다(`FlipflopRS.readInputs`). `@-480,2780`만 set되면 `run_int`가 바로 0이 되어 `INT_acp`가 나오지 않고 0x28c에서 stall합니다(16 seed 중 3회). 외부 IRQ는 `INT_MASKED` = 0이라 생기지 않습니다. → 사용자가 수정. 24 seed × tap 2종 = 48회 모두 PASS.
  - 다른 RS_FF에서도 같은 형태(S와 R이 같은 에지에 1)가 생기면 seed에 따라 결과가 달라집니다. seed마다 결과가 다르면 먼저 의심합니다.
- **남은 것:** 권한 검사는 배선되지 않았습니다(`ISA.md` 2.2 "권한 검사"). 그 뒤 추가된 테스트에서 나온 버그는 25~31번입니다.

## 25~31. 인터럽트 테스트 확장에서 나온 것 (11-18) ✅
tools 세션이 추가한 테스트(`int_sysprev`, `int_sysirq`, `int_nest`, `int_iret2`, `int_iretpre`, `int_wpsys`, `int_sysmem`, `int_flc`, `int_user`, `int_sys0`)입니다. `inttest.py -snap`(cpu-32 01:14) 결과는 PASS 32, XFAIL 11, SKIP 3이고, XFAIL 이유가 아래 번호입니다. 11-18 ~ 19에 모두 해결됐습니다(29번은 IRQ 장치 규칙으로, 나머지는 회로 수정으로. 그 과정에서 나온 것은 32~38번). 11-19 `inttest` 결과는 FAIL 0, PASS 43, XFAIL 2(20b, 20c), SKIP 0입니다.

- **25. `syscall` 반환 PC가 +4 (14a, 15c):** `D_FF@-200,1220`(HOLD)의 D는 clr ∧ **¬wait**입니다(`And@-280,1200`의 In_2가 반전). HOLD Q는 한 사이클 동안 `CT_BUS-_IAR`, `CT_RF-_PCSE`와 DC valid(`Register'DC'@60,960`)의 en을 켭니다. 즉 fetch를 한 칸 진행시킵니다. `FT`와 `DC`는 valid 비트이고, 주소는 `PCS` → `PCS_FT`(`Register@800,1700`, en = IAR) → `PCS_DC`(`Register@960,1700`, en = `dc_e`)로 갑니다. 진입 때 `int-pc`가 `PCS_DC`를 잡습니다.
  - int_sysprev case 5(예전 배치 `sp.bin`, `syscall` 0xd4, 정답 0xd8)에서는 이렇게 됩니다.
    1. 7182: wait(TRAP trap-wait, `Driver@1220,3460`)만 1입니다. FT가 꺼지고 DC valid ← 0이 됩니다.
    2. 7184: clr가 wait 없이 옵니다. HOLD D = 1이고, PCS ← 0xd8입니다.
    3. 7186: HOLD = 1이라 IAR이 켜지고, 7188에 `PCS_FT` ← 0xd8, PCS → 0xdc입니다. DC valid의 D = FT.Q ∧ ¬(clr|wait) = 0이라 0xd8은 DC로 들어가지 못합니다.
    4. 7192: FT가 다시 켜지고, 7194에 `PCS_FT` ← 0xdc로 덮입니다.
    5. 7196: `PCS_DC` = 0xdc이고, `int-pc` = 0xdc가 됩니다.
  - case 4는 wait와 clr가 같은 half(6952)라 HOLD D = 0입니다. PCS가 0xc0에 머물다가 FT 재시작 때 0xc0을 받습니다. 명령 종류가 원인이 아니라 타이밍 차이이고, 15c도 같은 형태입니다.
  - **고칠 방향:** HOLD D에 FT.Q를 더합니다(clr ∧ ¬wait ∧ FT.Q). FT가 이미 멈춰 있으면 한 칸 진행하지 않게 하는 것입니다. 대안은 DC valid D를 (FT.Q | HOLD) ∧ ¬kill로 바꾸는 것인데, trap 대기 중인 명령이 issue되지 않게 따로 막아야 합니다. HOLD가 원래 맡은 경우(halt-frontend와 겹칠 때 등)를 바꾸지 않는지 확인이 필요합니다.
  - 그림과 파형: https://claude.ai/artifact/PxT3wSw4RdRJyPuE1BGxqY
- **26. 공용 stack 가산기 (14d):** istack은 `addsub-4c32@2700,-180`을 stack과 같이 씁니다. 그 입력 mux `Mux@2620,-200`의 sel이 **clr**입니다(in_0 = `Mux@2520,-220`(sel `CT_ISCM`: SCS/ISC), in_1 = `CT_SC-_FD`). istack 갱신(`iret` +4, 진입 −4)이 clr와 같은 사이클이면 가산기가 `CT_SC-_FD`(stack)를 씁니다. int_sysprev case 4의 `iret`(half 7024) 뒤 istack = 0x3004, stack = 0x3004이고, NPAD 24개 빌드 모두 istack = 0x3004입니다.
  - **고칠 방향:** `CT_ISCE`가 켜져 있으면 그 mux가 clr를 따르지 않게 하거나, istack에 가산기를 따로 둡니다.
- **27. 핸들러의 `int-level`이 0x10 (14b, 20b):** `int_pri[1]` = 1인데 `syscall` 핸들러에서 읽은 `int-level`이 0x10입니다. level이 0이고, ker도 켜지지 않았습니다. 원인은 아직 보지 않았습니다.
- **28. 진입이 `int-tmplv`를 쓰지 않음 (16b):** 중첩된 핸들러가 읽은 `int-tmplv`가 0x81이 아니라 0입니다. `int-level` 0x80, `int-tmplv` 0x07로 두고 `syscall`해도 0입니다. 그래서 `iret`이 복원하는 level도 틀립니다.
- **29. `syscall` 직전 IRQ가 사라짐 (15b):** IRQ 펄스가 `syscall`의 `INT_req`보다 2~14 half 먼저 오면 그 IRQ가 들어가지 않습니다(33회, 모든 seed).
- **30. 두 번째 `syscall`의 `iret` (19a):** int_sysmem에서 두 번째 `syscall`의 `iret`이 첫 번째 반환 PC(0x320)로 간 뒤 stall합니다. 19b, 19c는 그래서 SKIP입니다.
- **31. `ld ; ld ; iret` (22, int_iretpre):** 이 순서에서 `iret`이 돌아오지 않습니다. 타이밍에 따라 달라서, int_iret 변형에서 `iret` 바로 앞이 nop이나 set일 때도 멈췄습니다. int_nest의 level 2 핸들러는 그래서 끝에 dummy st를 둡니다.
  - **원인 (11-18 trace, C4 정상 / C5 멈춤, zseed 1):** `iret`이 commit 자리에 오면 TRAP이 한 사이클 동안 `TRAP_IRET` 펄스를 냅니다(`Register@1100,4080`). 이 펄스가 Driver 13개(`Driver@340,4080~4560`)로 dispatch 제어 bus(`CT_FU-_INST/EPOCH/WR/MF`, `CT_MB-_LD`, `CT_MB-_OFFM`, `CT_INTM`, `CT_ISCM/ISCE/ISCS`, `CT_RF-_SCN`, `CT_ILVE`, `CT_RET-_LD`)를 직접 몹니다. 이 bus는 dispatch decoder의 Driver(sel = opcode decode ∧ `CT_DISP|disp_e`)도 같이 씁니다.
    - C4는 그 사이클(8306)에 halt-frontend = 1이라 dispatch가 쉬고 있어서 `iret` Driver만 켜집니다.
    - C5는 그 사이클(8332)에 halt-frontend = 0이고, `iret` 뒤에 fetch된 젊은 명령이 dispatch 중입니다. `Driver@1540,-1820`(`CT_FU-_WR`)과 `Driver@1540,-1900`(`CT_FU-_INST`)이 `iret` Driver와 같이 켜져 두 driver가 같은 net을 몹니다.
    - 그 뒤로는 클럭만 토글하고 회로의 모든 신호가 멈춥니다. istack 레지스터는 en = 1, D = 0x2000인데도 0x1ffc에 머뭅니다. 시뮬레이터는 아무 메시지도 내지 않습니다.
    - 앞 명령 종류가 원인이 아니라, 그 사이클에 dispatch가 쉬는지에 따라 갈립니다.
  - **고칠 방향:** `TRAP_IRET` 사이클에 dispatch를 막거나(disp_e = 0 또는 halt-frontend), `iret` micro-op을 dispatch가 비었을 때만 내게 합니다.
- **결정된 것 (11-19):** level 0 칸으로 `syscall`하면(int_sys0, 21b) IPU가 id 0(level 31)으로 바꾸고, CPU는 `int-level` 0x9f로 `int_vec[0]`에 진입합니다. 반환 PC는 `syscall` 다음 word입니다(`ISA.md` 2.2 "고정 id"). 11번(Fl_INT와 Fl_JPU가 같은 half)은 Fl_INT를 없애기로 해서 테스트에서 뺐습니다.

## 32~38. 25~31번을 고치며 나온 것 (11-19) ✅
재현은 `python tools/dsim/inttest.py`입니다. 좌표는 11-19 `cpu-32.dig` / `IPU-test.dig` 기준입니다.

- **32. `syscall` 두 번 진입 (8a, 8b, 14c, 15a, 16a, 20a):** run_int = ¬`RS_FF@-480,2780`.Q ∧ int-accept(`And@-480,2860`)입니다. INT_acp(4738)가 이 RS_FF를 reset한 뒤에도 int-accept가 한 사이클 더 1입니다. int-forced가 `D_FF@-500,3340`(INT_rqac)과 `D_FF@-500,3380`(INT_in)을 거치기 때문입니다. 그 사이클(4740)에 run_int가 다시 켜지고, 이미 INT_id = 0이라 vector 0으로 가서 프로그램이 처음부터 다시 돕니다. → 사용자가 수정(INT_acp를 한 사이클 늦춘 `D_FF@-280,3280`의 ~Q로 run_int 쪽을 막음). 그림: https://claude.ai/artifact/QdWJiiAuW2XpBL2QG5c6kr
  - 같은 무렵 진입의 level/level-T 쓰기가 int-accept 5사이클 내내 켜져 level-T가 새 level로 덮이던 것(27, 28번의 원인)도 첫 사이클만 쓰도록 고쳐졌습니다.
- **33. `iret` 뒤 commit 멈춤 (15a, 16a, 20a):** `iret`이 dispatch될 때 `CT_RET-_INT`로 켜는 `Register@4600,2940`의 bit가 자기 Q를 다시 받지 않아 다음 dispatch에 지워집니다. JPU가 그 bit를 1로 읽으면 `JPU_RETF` = 0이라 V1을 내지 않습니다(V1 = ¬MF ∨ RF). TRAP_IRET의 완료 표시(`bus16-demux@1240,4000`)는 V2에만 연결되어, id가 V1을 끝내 받지 못하고 commit이 멈춥니다. int_sys는 bit가 먼저 지워져 JPU가 일반 `ret`처럼 V1/V2를 다 내서 우연히 통과했습니다. → RET_INT register를 `(Q & cmks) | demux`로 유지하고 V1 출처를 하나로 정리(사용자 수정).
  - 처음에는 TRAP_IRET 사이클의 SCN 겹침을 원인으로 봤는데 틀렸습니다. SCN 겹침은 같이 dispatch된 push/pop의 stack 값을 틀리게 할 수는 있어도 멈추게 하지는 않습니다.
- **34. istack이 +4로 돌아오지 않음 (7b, 14d, 17b):** 가산기 결과 scs_i를 `Register@2640,20`(en = `CT_ISCM`)에 한 번 저장한 뒤 `Mux@180,3520`이 TRAP_IRET일 때 그 값(ISC_sc)을 istack으로 보냈습니다. TRAP_IRET 사이클의 ISC_sc는 이전 값(0x1ffc)이고, 0x2000은 쓰기 enable이 꺼진 다음 클럭에 들어옵니다. → 사용자 수정, 7b PASS.
- **35. IPU 출력 register 덮어쓰기 (15b, 29번의 원인):** 출력 register(`Register@1780,-100` id, `-20` vec, `60` lev, `220` rqit)의 en은 `Or@1660,140` = INT_acp ∨ INT_req ∨ 비어 있음입니다. IRQ를 내보내는 중(아직 수락 전)에 `syscall`의 INT_req가 오면 그 값으로 덮입니다. 입력 `D_FF@-320,-800`은 샘플만 해서 펄스 IRQ는 그대로 사라집니다. → 장치 쪽 규칙으로 해결: IRQ 선은 acp가 올 때까지 high를 유지합니다(`computer-32-test.dig`의 `RS-64`, S = I-in, R = I-acp). 그러면 `syscall` 수락 뒤 IPU가 IRQ를 다시 냅니다.
  - ⚠️ 남은 것: id register의 D는 INT_req로 채울 때도 T_num(`PriorityEncoder@260,-1120`)이라, `syscall` 수락 때 acp가 그 순간 T_num 선(보통 line 0)으로 나갑니다. 그 선에 장치가 high로 대기 중이면 핸들러 없이 clear됩니다. 테스트로는 아직 잡지 않았습니다.
- **36. FT RS_FF S=R=1 (15c, 1건):** `syscall`의 wait(FT R)와 IRQ 수락의 run_int(FT S, `Or@-240,960`)가 같은 클럭(17950)에 오면 `RS_FF 'FT'`가 S=R=1을 받아 무작위로 정해집니다(24번과 같은 형태). FT가 켜진 채 남으면 fetch가 0x42c → 0x438로 진행하고 int-pc = 0x430이 됩니다. → S = (run ∨ run_int) ∧ ¬wait(`And@-180,960`).
- **37. IRQ 수락 latch가 취소되지 않음 (16b, 18건):** 수락 latch `RS_FF@-240,2860`의 R은 run_int | INT_ISS ∨ INT_MASKED인데, INT_MASKED는 `syscall`(int-forced)일 때만 나옵니다. 낮은 핸들러의 close(`stexp 0x91`)가 commit돼 int-accept가 꺼져도 latch가 남아, 높은 IRQ가 close 뒤에 들어가거나(R = 0x1dc..0x1e4) `int-tmplv`에 0x91이 들어갔습니다. → 취소 조건 = (한 클럭 늦춘 com1) ∧ ¬(`Comparator@-320,3420`의 `>`)를 두 latch의 R에 넣고, INT_ISS의 And에 (¬com1-dff ∨ `>`)를 더함(사용자 수정, 16b PASS).
  - 고치는 중 두 번 틀렸습니다. ① com1 ∧ `>`(극성 반대): 수락 중 commit마다 latch가 풀려 진입이 ~1400 half 밀림. ② NAnd(com1, `>`): commit이 없을 때도 1이라 latch가 거의 항상 reset. ③ com1 ∧ ¬`>`: 비교기가 `stexp` commit 다음 클럭에야 새 level을 봐서 그 클럭의 INT_ISS를 막지 못함(3건) → com1을 D_FF로 늦추고 INT_ISS를 막아 해결.
- **38. ICLR 창의 펄스 (17b):** acp 뒤 IPU의 ICLR(`NOr@1360,-160`, INT_acp와 2단 지연)이 3클럭 동안 새 요청을 막아서, 그 안에 온 1-clock 펄스가 사라졌습니다. → 35번의 장치 규칙(acp까지 high)으로 해결. 다만 acp가 오는 바로 그 edge(iret2의 H2 = 13970)에 펄스가 들어오면 `RS-64`가 S=R=1을 받아 seed에 따라 잡히기도 하고 무시되기도 합니다. 17b는 그 경우 진입 1번과 2번을 모두 허용합니다(`I-act|I-acp`로 판정).
  - 13:50 회로와 13:57 회로에서 같은 펄스가 잡히고 안 잡힌 차이는 INT_acp와 ICLR 시점이 같아서 그쪽 때문이 아니고, 원인은 확인하지 않았습니다.

## 39~53. kernel 시작, syscall 경로, page/privilege fault (11-22 ~ 11-25) ✅
재현은 `python tools/dsim/inttest.py` (fault는 그룹 `fault`, 23a~23h, `program/test/interrupt/int_fault.asm`)입니다. 좌표는 11-23 `cpu-32.dig` 기준입니다. 11-23 기준 회로: page fault(inst-read, data store/load의 MMU error) = id 3, privilege fault = id 4. 우선순위: fetch page fault > privilege fault > data page fault > syscall/trap(break). syscall/iret 중 data fault는 아직 처리하지 않음.

- **39. `CT_INT-_W`/wait driver 충돌 (11-22):** "more than one output active on a wire"(dsim이 이제 `stop: ERROR`로 잡음). → 사용자 수정 ✅
- **40. `index-compressor` SI4..7 위치, ROB-INT data select (11-22):** syscall의 ROB-INT 쓰기가 사라지거나 id 0으로 진입. → 사용자 수정 ✅
- **41. flush 사이클에 issue된 오경로 serialize 명령이 SRL 래치를 남김 (11-22, 14a, 14c, 15a, 16a, 19a):** `RS_FF@5160,-2600`(SRL_HALT)의 S = `CT_ISS-_SRL`, R = clr ∧ inst-sfcomp(저장된 이전 id). 오예측 flush(clr)와 같은 사이클에 오경로 `ldexp`/`iret`이 issue되면 R이 이전 id를 봐서 0, S만 들어가 frontend가 영원히 멈춤. 최소 재현 `jmp.one main; nop; ldexp; iret; main:`. → 사용자 수정 ✅
- **42. fault의 R(복귀 PC) (11-23, 23a~23f):**
  - 처음: E-clr이 PC를 `HALT_PC`(Register@1580,4240, en = `CT_TRAP-_LD`, TRAP 유닛 명령의 PC만 담음) + 4로 복구. fault 때는 갱신되지 않아 마지막 kernel `iret`(0x810) + 4 = 0x814가 R이 됨(`int-pc` = INT_ISS 때의 `PCS_DC` = 복구된 fetch PC). 예전 int_user의 같은 store 무한 반복도 같은 원인.
  - 1차 수정(22:05): commit 중인 명령의 PC(`pca_i`, ROB-PC D1)로 복구했지만 +4가 붙어 R = 명령 + 4.
  - 2차(22:08): 모든 E-clr이 +4 없이 `pca_i`로 가서 TRAP 완료(`interst` 등)가 자기 자신을 다시 실행, 프로그램 시작에서 무한 반복.
  - 3차(22:17): PCF = `Fl_ECLR` ∧ `CT_INT-_W`(And@280,1920) → `pca_i`, +4 없음. fault R은 맞음(23a, 23b PASS, 23c R 맞음). **그러나 syscall/trap도 ROB-INT에 기록돼 `CT_INT-_W`가 켜지므로 syscall R = syscall 자신이 되어 무한 재진입**(8a/8b/14/15a/19a/20/21b/22 FAIL, inttest PASS 33 FAIL 15). → PCF(+4 생략)는 fault(id 3, 4)일 때만, syscall/trap(id 1, 0)은 +4. → 사용자 수정(11-24) ✅
- **43. privilege fault가 난 명령이 그대로 실행됨 (23c, 23e):** → 47, 48번으로 해결 ✅ 검사 And@-280,2380 = mode(¬ker) ∧ `ker_DP`의 출력은 ROB-INT 기록(`bus16-demux@-180,2380`)과 int-err(`Or@-180,2320`)로만 가고 유닛 dispatch enable을 막지 않음. user `stexp istack`은 istack = 0x1234를 써 버리고(push가 0x1230), user `iret`은 TRAP이 실행해 mem[istack]의 SENT(0x5A5A5A5A)로 JPU flush를 내며 fault entry와 겹쳐 `waiting=1`에서 멈춤. 방향: 검사 결과로 dispatch를 막거나, 부작용 있는 유닛(확장 레지스터 쓰기, TRAP)이 같은 id의 ROB-INT 기록을 보고 실행하지 않게. ❌
- **44. fetch page fault entry가 멈춤 (23d):** fetch fault는 dispatch 때 ROB-INT에 기록되어 `CT_INT-_W` → `INT_req` → `INT_rqac`가 ROB-gen의 commit(`Fl_ECLR`)보다 먼저 지나감. 그 뒤 `waiting=1`, `INT_fetch=1`인 채 `INT_HALT`로 가지 못함. 정상인 data fault는 `Fl_ECLR` → `INT_req` 순서. → 49번(INT_req를 ECLR 뒤로)과 51번으로 해결 ✅
- **45. flush 사이클에 issue된 오경로 kernel 명령이 ROB-INT에 privilege fault를 남김 (23f):** 41번과 같은 종류. BTB miss `jmp` flush(clr)와 같은 사이클에 오경로 `stexp`(또는 `iret`)가 issue되고, 다음 사이클에 flush된 id로 `ROB-INT_D=4`가 쓰임. 핸들러에서 그 id가 재사용될 때 발동해 가짜 중첩 V(tmplv 0x91). 권한 검사의 ROB-INT 쓰기도 ¬clr로 막아야 함. 23g(오경로만)은 타이밍이 달라 PASS. → 50번으로 해결 ✅

11-24 ~ 11-25 (좌표는 11-24 `cpu-32.dig`):

- **46. fault 난 load의 FLT 비트가 clear 뒤 다시 켜져 다음 load가 물려받음 (23a, 23f):** load-buffer slot의 FLT는 `W_d` ∧ ¬`W_FWD` ∧ (`mid_WD` == slot mid) ∧ `fault`일 때 저장. 그런데 `computer-32-test.dig`에서 cpu-32의 `D_responce`가 자기 `D_LD` 출력과 같은 net이었고, MMU-D가 fault 때 `ld_o`/`D_recieved`를 주지 않아 `D_LD`(= `D_responce`)가 다음 MMU 접근까지 1로 남음. `MEM_LD-_RD`(RS_FF@800,2340, S = `D_responce`)와 `W_d`, `mid_WD`, `fault`가 모두 level이 되어 Fl_ECLR의 slot clear(slot mid는 지우지 않음) 바로 다음 half에 FLT가 다시 1. 핸들러의 `ld LOGP`가 같은 slot을 받아 MMU 오류 없이 page fault(R = 0x14). 붙잡힌 `D_LD`는 다음 MMU 접근의 변환으로 메모리까지 나감. → 사용자 수정(`computer-32-test.dig` 11-24 22:48) ✅
- **47. 확장 레지스터 쓰기가 commit을 보지 않음 (23c):** `RX_ISCE`/`ILVE`/`ILVTE`/`PGBSE`/`PGLME` = demux(`And@-500,4140`(`CT_RF-_W1`, `CT_RF-_EX1`), `CT_RF-_R1`)(포트 2도 같음). regFile 쓰기(`And@1400,-200`)와 달리 `com1`/`com2`가 없어 fault로 commit이 막힌 user `stexp istack`이 Fl_ECLR과 같은 half에 istack = 0x1234를 씀. → com1/com2 추가 ✅
- **48. user `iret`이 TRAP 유닛에서 실행됨 (23e):** `TRAP_IRET`이 commit 전에 istack pop(0x2000 → 0x2004), int-level 복구, 복귀 PC fetch(0x5A5A5A5A). → 사용자 수정 ✅
- **49. `INT_req`가 Fl_ECLR보다 먼저 나감 (23d, 23e):** `INT_req` = D_FF@-560,2440(D = ROB-INT `W_O`). `W_O`는 head의 ROB-INT 기록만 보고 켜지고 Fl_ECLR(id-manager `eclr_v`)은 head가 commit 가능해야 켜짐. 앞서 나가면 IPU의 `rqac`가 `waiting=1`보다 먼저 와서 entry가 멈춤. → ECLR과 함께 나가게 수정 ✅
- **50. privilege fault 쓰기에 dispatch 조건이 없음 (23f):** ROB-INT 포트 4 = `Or@-140,2320`(fetch fault `And@-240,2460`(`CT_DISP`, `fild_DP`), privilege `And@-240,2380`(`MMU_mdc`, `ker_DP`)). privilege 쪽에 `CT_DISP`가 없어 dispatch가 멈춘 half(분기 flush 다음)에 res-station 출력에 남은 오경로 `stexp`(이미 clr된 id 8)의 fault가 기록됨. ROB-INT 안의 clear 우선은 맞게 동작했고, 쓰기가 그 id의 clear보다 한 half 늦게 와서 막을 clear가 없었음. 45번의 실제 원인. → `CT_DISP` 추가 ✅
- **51. fetch fault의 R이 0x6008, 그다음 issue/commit (23d):** fetch fault 명령이 issue되지 않고 decode PC(`PCS_DC`)만 진행해 fault 표시가 두 word 뒤 PC로 ROB-PC에 들어감. `fault_ILD`를 decode에 저장해 issue를 막자 fault 명령 자신도 issue되지 않아 기록이 생기지 않음 → fault 명령을 강제 issue, 그러나 ROB-gen이 done이 되지 않아 Fl_ECLR이 나지 않음 → dispatch에서 fault 명령을 강제 validate. ✅
- **52. id-manager 출력 핀이 한 칸 밀림 (11-24 23:41):** id-manager에 `fault1` 출력을 더하고 위치를 옮기면서 `cmks_16` 핀에 `icmks_16`와 `clrs_16` tunnel이 함께 붙고 `clrs_16` 핀은 빈 선(@-340,1740). ROB-gen/ROB-INT/ROB-SC/ROB-FL의 `clr_16`에 commit mask가 들어가 boot에서 `block`이 copy loop를 기다리지 않고 `jmp gen[4]`(0)로 감. 모든 테스트가 실패. → 배선 수정 ✅
- **53. commit slot 2에 온 fault (23e, 51번 뒤):** 강제 validate로 user `iret`(id 5)이 앞 `set`(id 4)과 같은 사이클에 commit에 옴. ROB-INT `W_O`(`CT_INT-_W`)는 slot 1만 봐서 0(`W2_O` = 1), id-manager는 ECLR을 내고 복구 PC는 slot 1의 `pca_i`(0x83C)라 `set`과 `iret`을 무한 반복. → 사용자 수정(`ROB-INT.dig` 11-25 00:35) ✅

## 54~59. store/load buffer, ROB-SC, alignment fault, `pop flag` (11-27)
재현: `bash tools/dsim/runtests.sh ststld knownbug ldstmix straddle`, `python tools/dsim/inttest.py align`. 11-27 끝에 runtests SAME 16, inttest PASS 58 (59번 제외). 접근 규칙은 자연 정렬로 정함: half는 짝수, word는 4의 배수 주소, byte는 어디나. 아니면 fault 5.

- **54. ROB-SC self flush 복구 범위 (ststld):** 복구할 entry를 고르는 mask C(≥ com1)와 R(≤ roll', self flush면 roll - 1)에서 wrap = roll' < com1을 `Comparator@3240,-140`("= 0", C ∧ roll' one-hot)로 구함. 처음 ≥ 비교로 바꿨을 때 roll == com1(자기 자신이 아닌 경우)이 깨짐(재시작 120회). mask 방식에서는 `Not@3300,-160`이 wrap을 뒤집어 다른 entry를 고름(12736 half, clr_id = a, com1 = 8에서 entry c, 재시작 69회). 빈 범위(roll_sf ∧ com1 == roll)는 SCA를 그대로 씀. → Not 제거 ✅
- **55. forward 폭 (17번, knownbug k1~k3):** store-buffer의 `M*_CMP`가 같은 word만 보고 store byte mask를 보지 않았음. store byte mask를 MB_MD에서 만들고(`MB_SET`: MD 0 = 32B 1111, 1 = 16L 0011, 2/3 = 8L/8H 0001을 `MB_addr[1:0]`만큼 shift, `Multiplexer@2480,4480`), 고른 slot의 mask ⊇ load mask일 때만 forward(`Comparator@660,-160`). 아니면 over-clr(`And@800,-140`) → `STB_clr` → MB_INST replay. ✅
- **56. 다음 word 비교 (ldstmix m4):** 두 word에 걸친 load를 위해 `M*_CMP1`(comp_addr + 1)을 더했는데, 정렬 load에도 받아들여짐(4만큼 shift한 load mask가 0이라 어떤 store mask에도 포함). → `ld_high` 조건 추가 ✅. 자연 정렬로 정한 뒤로는 걸친 접근이 fault라 이 경로는 쓰이지 않음.
- **57. forward data byte offset (straddle a, c, e, f):** 같은 word의 다른 offset에서 읽는 load(`st.32B X` 뒤 `ld.8L X+1`)가 store 데이터를 shift 없이 받아 byte 0(`CC`)을 읽음. shift를 넣은 첫 판은 방향이 반대라 offset 1 ↔ 3이 뒤바뀜(`ld.8L X+1` → 99, `ld.8L X+3` → BB, `ld.8H X+1` → 9900, `st.16L X+2` 뒤 `ld.8L X+3` → 00). offset 2는 방향과 상관없이 맞아서 통과. → 사용자 수정 ✅ (straddle a~n 모두 PASS)
- **58. alignment fault (id 5, int_align 24a~24f):** `cpu-32` (3300~3640, 3920~4120). misal = MB_any_e ∧ MD[1] ∧ (MD[0] ? a0 : a0|a1)로 MD 인코딩(0 = 32B, 1 = 16L, 2 = 8L, 3 = 8H)과 맞지 않아 8L/8H가 fault, 32B/16L은 통과 → 문자열을 `ld.8L`로 읽는 selftest/calc/picalc가 멈춤. → `And@3520,4080` In_2 반전(¬MD[1]) ✅. fault id driver(`Driver@3640,4060`)가 두 단 뒤의 `MMU_INST`를 씀 → `MB_INST` ✅. fault 난 store는 메모리를 바꾸지 않음(24c, 24d).
- **59. `pop flag` 바로 뒤 ALU가 flag를 못 봄 ✅ (11-28; 다른 세션이 dsim으로 찾음, 테스트 `program/test/flagpop.asm`):**
  - `set gen[3], 8 ; push gen[3] ; pop flag ; addc gen[1], gen[2]`(0 + 0) → 0 (정답 1, carry를 못 봄). 뒤의 `push flag`는 0x48(pop한 값, `addc`의 flag가 사라짐). 사이에 `nop` 하나 있어도 같음.
  - `pop flag`(0) `; cmp 0,1 ; push flag` → 0x40 (정답 0x4A, `cmp`의 N, C가 사라짐).
  - 사이에 실제 명령(`set` 등)이 하나 있으면 맞음. `nop`은 dispatch 전에 빠지는 것으로 보임.
  - 별도 관찰: flag = 8(carry만)을 pop한 뒤 `jmp.pos`가 taken(bit 2 = 0인데). pos를 z/n에서 만드는 것으로 보임.
  - 영향: ISR 끝의 `pop flag` 바로 뒤가 ALU 명령인 경우.
  - 원인 1 (투기 경로, flagpop d, e): `addc`가 pop한 carry를 기다리지 않고 commit된 FL을 읽음. `inst-wfile`의 flag 생산자 추적이 `pop flag`(reg 1F 레지스터 쓰기)를 flag 생산자로 보지 않았음. → 사용자 수정(`inst-wfile.dig`) ✅
  - 원인 2 (commit, flagpop a, b, f~i): regFile의 FL 쓰기 선택 `sel = FL_sel ∧ ¬(E1 ∧ A1 == flag) ∧ ¬(E2 ∧ A2 == flag)`(`And@1020,2200`, `NAnd@880,2180/2260`). 어느 포트든 flag에 레지스터 쓰기가 있으면 ROB-FL의 ALU flag를 버림. slot 1 = `pop flag`, slot 2 = `cmp`가 같은 사이클에 commit되면(half 10310, `com1 = com2 = 1`, `E1 = 1`, `CT_FL-_W2 = 1`) 더 젊은 cmp의 flag가 사라짐. case g의 `jmp.neg`는 그 뒤에 풀려 망가진 FL을 읽음(18066 commit, 18070 not taken). `f`(cmp가 늦음), `i`(pop의 load가 늦음)도 DELAY 뒤에서 한 쌍으로 commit됨. `set`이 사이에 있으면(c) cmp가 다음 사이클에 따로 commit되어 맞음. → regFile 입력 `FL_W2`(cpu-32 `And@3500,-100`, slot 2의 ALU flag 쓰기)를 더해 slot 1 레지스터 쓰기는 `¬FL_W2`일 때만 이기게 함 ✅
  - stack도 같은 구조(SCA, SCS의 `NAnd@740,1860/1940`, reg 1E)라 `SCS_W2`(cpu-32 `And@2000,-120`)로 같은 처리. 고치기 전 회로(3f1c4f9)에서도 `program/test/memory/stackmov.asm` 12 case는 에뮬레이터와 같았고, stack 레지스터 쓰기 26 half 중 push/pop commit과 겹친 것은 반대 순서(slot 1 push, slot 2 `mov stack`, half 35554) 한 번뿐이라 문제 순서는 나오지 않았음.
  - 같이 볼 것: ALU flag 경로 `fl_i`는 ROB-FL bit 0–7, bit 8 = FL[14], bit 9–31 = Ground이고 `FL_e-_EX` = 1이라, ALU commit마다 범용 flag bit(08–1F)가 지워지는 것으로 보임(확인 안 함).
  - 11-28 확인: flagpop a~i zseed 1~3 PASS, runtests SAME 18, inttest PASS 58.

## 60. hex 출력 루프 deadlock (11-27) ✅ (11-28)
다른 세션이 carry 테스트를 만들다 찾음. 11-28 kernel(1fc95c0, `inst-wfile.dig` 작업 중)에서 다시 확인: zseed 1, 2 모두 `PC=214 stop: stall 2000`(92678 half). 에뮬레이터는 끝까지 실행(`[14507 steps]`).
- 재현: `carry-loopshow.asm`(carry.asm에서 출력만 루프로 바꾼 판). 아래 루프를 `call`로 부름(gen[17] = 값, gen[24] = IO_BASE).
  ```
  show_loop: mov gen[20], gen[17] ; shiftr gen[20], gen[19] ; bnd gen[20], gen[18](0xF)
             cmpi gen[20], 10 ; jmp.neg show_num ; addi gen[20], 'A'-10-'0'
  show_num:  addi gen[20], '0' ; st.gen[24] gen[20], TERM_OUT
             cmpi gen[19], 0 ; jmp.zero show_end ; subi gen[19], 4 ; jmp.one show_loop
  ```
- 출력: `a0 Z..C4. 00000000`, `a1 ..P... 00000003`까지 맞고 `a2 .N..4V 8`(0x7FFFFFFF + 1 = 0x80000000의 첫 자리)에서 멈춤. 앞의 두 case는 루프 8번을 다 돎.
- **원인:** JPU 오분기 flush와 같은 사이클에 JPU로 들어간 오경로 `ret`이 죽지 않고, 다음 사이클에 풀려 자기 flush를 냄(41, 45, 50번과 같은 종류). zseed 1, a2의 둘째 자리:
  - 90644: `jmp.zero show_end`(0x39f8, ID 3)가 taken 예측(BTB가 앞 루프에서 배움)인데 not taken. `Fl_JPU` = 1, `clr_id` = 3, `clr_ep` = 1. 같은 half에 `CT_JPU-_LD` = 1로 뒤의 오경로 `ret`(0x3a04, ID 4)이 JPU에 load됨.
  - 90646: JPU가 그 `ret`을 풀어(`JPU_EN` = 1, `JPU_PC` = 3a04) miss로 다시 flush(`clr_id` = 4, `clr_ep` = 1). epoch가 같아(`epoch_DP`는 90676에야 바뀜) 걸러지지 않음.
  - 90648: PC가 반환 주소 0x1d8로 감. 루프 나머지가 버려진 채 a2 코드를 돌다 0x214에서 멈춤.
  - ret 경로(`D_FF@5220,2780` → `Driver@5420,2800..2920`가 `CT_JPU-_LD`, `RET_INST`/`RET_EPOCH`를 JPU로 모는 곳)에 `clr_id`/`clr_ep` 비교가 없었음. 다른 단계(`inst-sfcomp`)와 달리 JPU 입력 쪽(`D_FF@4420,980` `JPU_EN`, `Register@4420,1160`)도 clr로 죽지 않음.
  - a2에서만 난 이유: `ret`이 `jmp.zero`가 풀리는 바로 그 사이클에 JPU로 갈 준비가 된 타이밍. 값의 bit 31이나 flag와는 상관없음.
- **수정 (cpu-32):** `inst-sfcomp@5080,2620`(A = `RET_INST`/`RET_EPOCH`, B = `clr_id`/`clr_ep`/`clr_self`)의 `A_new`와 `clr`로 `NAnd@5220,2680`, 그 결과와 ret 유효(`D_FF@5220,2780`)를 `And@5300,2740`로 묶어 ret driver의 sel로 씀. flush보다 젊은 ret은 그 사이클에 JPU로 load되지 않음.
- **확인:** carry-loopshow zseed 1, 2 에뮬레이터와 같음(100줄, `ok`). runtests 18개 SAME, inttest PASS 58.
- `program/test/carry.asm`은 루프 대신 풀어 쓴 출력을 그대로 씀. 재현 파일은 `program/test/carry-loopshow.asm`.

## 기존 알려진 문제: `halt`가 분기보다 먼저 실행 (10-17) ✅ 재현 안 됨 (11-07)
- **예전 증상:** not-taken으로 예측된 분기 바로 뒤 `halt`가 분기보다 먼저 실행돼 CPU가 멈춤. 그래서 프로그램을 `jmp.one pc.32B, 0`으로 끝냈음.
- **테스트 `program/test/haltbr.asm` H1~H10:** 처음 실행이라 not-taken으로 예측되고(JPU_pred=3) 실제로는 taken인 분기 바로 뒤 halt. 분기 앞에 상수 cmp, 24단 addi chain, load, pointer chase, self flush되는 load, load한 카운터 루프, 늦은 taken 분기, load한 레지스터로 jmp.one, load 8개 진행 중, 먼 target. 10개 × zseed 1~3 모두 에뮬레이터와 같음. halt를 넣은 fill, picalc도 같음.
- **이유 (dump H2/H4/H6):** wrong-path halt는 분기보다 먼저 issue되지만(H4: halt 0x30이 1024에 `CT_ISS_HALT`, 분기는 1074에 풀림) CPU는 멈추지 않음. halt가 commit 때 효과를 내는 구조라서(11-05 "trap/halt 뒤 명령 flush") issue 순서가 문제 되지 않음.
- **남은 관찰:** flush 뒤에도 `hlt`=1, `HALT_PC`가 옛 halt 주소로 남음. 지금은 영향 없지만 인터럽트/`INT_HALT`에서 쓰면 확인 필요.

---

## 도구/환경 쪽 문제

| 문제 | 증상 | 해결 |
|---|---|---|
| 클럭 DipSwitch 위치 | dsim이 클럭을 못 넣어 바로 끝남 | `-580,-1640` → `-560,-1640`. 지금은 dsim 기본값, 틀리면 에러(`8f31886`) |
| 키보드 클럭 | 키가 하나 걸러 빠짐 | `C & D_ST` 클럭 glitch → `C` + `en` (`bf78b59`) |
| 테스트 중 회로 저장 | 결과가 섞임 | 테스트하는 동안 Digital에서 저장하지 않기 |
| Digital에 열린 옛 회로 | 외부 수정이 덮어써짐 | 파일을 바꾼 뒤 Digital에서 다시 불러오기 |
| `boot/rom.bin` 옛 형식 | GUI 부팅이 안 끝남(start=0x9415f01a, 길이가 큼) | dlnk 형식(헤더 있음) 이미지 넣기. runtests는 `-Drom`이라 상관없음 |
| runtests 경로 | `ClassNotFoundException: Sim` | java에 `pwd -W` Windows 경로 |
| 에뮬레이터 입력 | `q`에 줄바꿈이 없으면 step 한도까지 돎 | 키 끝에 `\n` |
| picalc `100 10` | 4M half 안에 안 끝남(약 426만 명령) | `10 10`으로 줄임 |
