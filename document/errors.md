# 회로 에러 기록

INT-MMU / buff-outstanding 작업(2026-10-29 ~ 11-05) 중 dsim으로 찾은 회로 에러를 모아 둔 문서입니다. 비슷한 증상이 다시 나오면 여기서 먼저 찾아봅니다.

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
| 11 | 11-04 | self flush된 load commit | 이전 값으로 commit될 수 있음 | ⚠️ |
| 12 | 11-04 | load-buffer OLDD | memtest2 `a7`이 `7070` (정답 `7171`) | ✅ |
| 13 | 11-04 | load-buffer 응답 매칭 | picalc `pop`이 다른 엔트리 값을 받음 | ✅ |
| 14 | 11-04 | 버스 mid | store 응답이 load 데이터로 들어감 | ✅ (store 응답 끔) |
| 15 | 11-04 | 프론트엔드 flush | calc `ret`이 0x40a00000으로 점프, picalc 끝자리 틀림 | ✅ (11-05 최종 수정, flushstall 통과) |
| 16 | 11-02 | res-station.dig (non-debug) | `inst-wfile` 핀 연결이 밀림 | ❌ |
| — | — | 기존 알려진 문제 | `halt`가 분기보다 먼저 실행, `st .8L`이 4바이트 씀 | ❌ (CLAUDE.md, `knownbug`) |

2026-11-05 기준 runtests 9개(selftest z1~3, memtest, memtest2, fill, calc-halt, calc, picalc) 모두 에뮬레이터와 같습니다.

회귀 테스트(`program/test/`, 설명은 `program/document/test.md`):
- `flushstall`: 15번 (store K개 + nop N개 뒤 call/ret/jmp/분기 flush) → 모두 PASS (11-05 최종 수정 후)
- `ldstmix`: 13·14번 (load/store 섞기, 연속 load, pointer chase) → 모두 PASS. store 응답을 다시 켜면 재확인
- `knownbug`: `st .8L/.16L/.8H`, halt 직후 분기 → DIFF가 정상 (k1~k3 FAIL, halt 문제는 이번엔 재현 안 됨)

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

## 11. self flush된 load가 이전 값으로 commit될 수 있음 (11-04) ⚠️
- **관찰:** self flush 뒤에도 load id의 완료 비트(`V1_inst`)가 남아 앞 store와 함께 commit됨(com2). 엔트리도 비워져 replay되지 않음.
- **필요한 것:** self flush 때 load id의 완료 비트를 지우고, ROB 결과를 replay가 다시 덮어쓸 수 있어야 함.
- **상태:** 이후 테스트는 모두 통과하지만, 이 경우가 생겼는데 값이 우연히 맞았는지는 확인하지 않았습니다.
- 같은 dump에서 "ROB head id 5가 `DV=1`인데 완료가 안 됨"도 보였으나 원인은 안 봤습니다(이후 재현 안 됨).

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
- `res-station-debug.dig`처럼 맞추거나 `sync-nondebug.sh`로 맞춰야 합니다. (self flush 구조가 바뀌어 `clr_self`가 없어졌다면 다시 확인)

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
