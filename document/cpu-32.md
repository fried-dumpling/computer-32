# cpu-32 구성 정리

`components/cpu-32.dig`를 정리할 때 참고하려고 만든 문서입니다. 현재 구성, 외부 핀, 시작 순서, 회로 영역, 하위 모듈, 미완성 부분, 정리 후보를 기록합니다.

> 2026-10-26 기준으로 저장소의 회로 파일을 직접 조사한 결과입니다. 회로가 바뀌면 숫자와 좌표는 달라집니다.
> 다시 조사할 때는 `tools/dsim`의 `NetQuery`, `Pins`, `NetSig` 도구를 씁니다. 사용법은 `tools/dsim/README.md`에 있습니다.
> `(?)` 표시는 회로만으로 의미를 확정하지 못한 항목입니다.

## 1. 파일 관계

| 파일 | 역할 |
|---|---|
| `cpu-test.dig` | 개발용 CPU입니다. 실제로 고치는 원본이고, probe/LED와 debug 모듈(`*-debug.dig`)을 씁니다. 왼쪽(x < -1200)에 메모리/IO가 붙어 있어 이 파일 하나로 dsim에서 실행됩니다. |
| `cpu-32.dig` | 부품용 CPU입니다. `cpu-test.dig`에서 probe/LED와 메모리/IO 영역을 빼고, 모듈을 non-debug 버전으로 바꿨습니다. 외부 핀 35개가 있습니다. |
| `computer-32.dig` | 시스템입니다. `cpu-test`의 메모리/IO 영역에 `cpu-32` 인스턴스 하나를 터널로 연결했습니다. |
| `computer-32-test.dig` | 작업 중인 파일입니다. 지금은 `B_used` 터널에 구동이 없다는 오류로 로드되지 않습니다(이전 `computer-32.dig`로도 같은 오류). |
| `*-debug.dig` ↔ non-debug | `id-manager`, `res-station`, `load-buffer`, `store-buffer`가 대상입니다. non-debug 버전은 debug 버전에서 probe와 디버그용 Out을 뺀 것입니다. |

`tools/dsim/sync-nondebug.sh`는 debug 모듈에서 non-debug 모듈 4개를 다시 만들고, `cpu-test.dig`에서 `cpu-32.dig`와 `computer-32.dig`를 다시 만듭니다. 그다음 넷리스트를 비교합니다.

**주의 (2026-10-26)**: `cpu-32.dig`(10-25 23:50)와 `computer-32.dig`(23:51)는 생성된 뒤 직접 수정되었습니다(아래 §3, §4). 지금 `sync-nondebug.sh`를 실행하면 이 수정이 사라집니다. `cpu-test.dig`에도 같은 변경을 넣거나, 스크립트에서 이 두 파일을 만드는 단계를 빼야 합니다(§9).

## 2. 규모

`cpu-32.dig` top level에는 소자 3348개와 wire 5278개가 있습니다.

| 종류 | 개수 |
|---|---|
| Tunnel | 1809 |
| Driver | 238 |
| And / Or / Not / NAnd / NOr / XOr | 199 / 75 / 36 / 12 / 11 / 7 |
| Splitter | 184 |
| VDD / Ground / Const | 150 / 58 / 32 |
| PullDown | 114 |
| Register / D_FF / RS_FF | 107 / 40 / 26 |
| Multiplexer / Demultiplexer | 75 / 4 |
| Rectangle (영역 표시) | 53 |
| 하위 회로 인스턴스 | 56 (파일 28종) |

## 3. 외부 핀

pin 좌표는 `cpu-32` 모양 안의 위치(0..320)입니다. "내부"는 cpu-32 안에서 그 핀을 쓰는 곳의 수입니다(소비 = 그 넷을 입력으로 받는 핀 수).

### 왼쪽: 제어

| 핀 | 방향 | 비트 | 위치 | 내부 | 설명 |
|---|---|---|---|---|---|
| `C` | 클럭 | 1 | 0,40 | 넷 `C_dt`/`C_i` | 안쪽 Clock 소자(-480,-1640)가 하위 회로에서는 입력 핀 역할을 합니다. 제어 레지스터는 `Not`(-520,-480)으로 반전한 `C_ct`를 씁니다. |
| `E` | in | 1 | 0,100 | 소비 1 | 0이면 `Not`을 거쳐 `res-station.res`를 리셋합니다. |
| `run` | in | 1 | 0,160 | 소비 2 | RS_FF `FT`(fetch, `run_int`와 Or)와 RS_FF `HLT`(~Q = `halted`)를 set합니다. |
| `boot` | in | 1 | 0,220 | edge 검출 | D_FF 두 개와 And로 상승 에지에서 1클럭 펄스 `boot`를 만듭니다. 이 펄스가 PC 선택(PriorityEncoder@1460,980 in3)으로 `reset-vec`를 고릅니다. |
| `reset-vec` | in | 32 | 0,280 | 소비 1 | boot 때 PC가 됩니다. computer-32 기본값은 0xFFFFFF00(boot ROM)입니다. |

### 오른쪽: 메모리 버스

| 핀 | 방향 | 비트 | 위치 | 내부 | 설명 |
|---|---|---|---|---|---|
| `I_valid` | in (3-state) | 1 | 320,60 | 소비 3 | 명령어 메모리 준비 신호입니다. computer-32에서는 ram-test가 대기 중 0을 내고, PullUp 때문에 평소에는 1입니다. |
| `I_D-_bus` | in (3-state) | 32 | 320,80 | 소비 2 | 명령어 데이터입니다. `Register'I_D-_R'`(1500,1560)에 latch되며, enable은 `dc_e`입니다(§7). |
| `I_A-_bus` | 3-state 버스 | 32 | 320,100 | `Driver`@1260,1580 | 명령어 주소입니다. |
| `I_LD` | out | 1 | 320,120 | `Driver`@-500,160 | 명령어 읽기 요청입니다. |
| `B_used` | in | 1 | 320,140 | 소비 2 | (?) 버스를 다른 장치가 쓰는 중이라는 뜻으로 보입니다. computer-32에서는 기본값 0인 In입니다. |
| `D_accept` | in | 1 | 320,160 | 소비 1 | (?) 데이터 메모리가 요청을 받았다는 신호로 보입니다. computer-32에서는 기본값 1인 In입니다(메모리/IO는 이 핀을 구동하지 않습니다). |
| `D_D-_bus` | 3-state 버스 | 32 | 320,180 | `Driver`@1580,2780 | 데이터입니다. store 때는 CPU가 구동하고, load 값은 `Register`@1460,3060이 받습니다. |
| `D_A-_bus` | 3-state 버스 | 32 | 320,200 | `Driver`@1580,2620 | 데이터 주소입니다. |
| `D_LD` | out | 1 | 320,220 | `And`@1600,2260 | load 요청입니다. |
| `D_ST` | out | 1 | 320,240 | `And`@1600,2340 | store 요청입니다. |
| `D_MD` | out | 2 | 320,260 | `Driver`@1580,2700 | 접근 폭입니다. 0 = 32, 1 = 16, 2 = 8, 3 = u.u(핀 설명 기준). store D_MD가 항상 0인 버그가 있습니다(CLAUDE.md "Known circuit problems"). |

### 위쪽: 인터럽트 (IPU 연결 예정, `IPU-test.dig`)

| 핀 | 방향 | 비트 | 위치 | 내부 |
|---|---|---|---|---|
| `INT_in` | in | 1 | 40,0 | 소비 3 |
| `INT_ker` | in | 1 | 60,0 | **안 씀** |
| `INT_lev` | in | 4 | 80,0 | 소비 2 |
| `INT_vec` | in | 32 | 100,0 | 소비 1 |
| `INT_acp` | out | 1 | 120,0 | `And`@-160,1520(넷 `INT_ISS`) |
| `INT_id-_bus` | in (3-state) | 6 | 140,0 | 소비 1 |
| `INT_d-_bus` | in (3-state) | 32 | 160,0 | 소비 1 |
| `INT_rqac` | in | 1 | 180,0 | **안 씀** |
| `INT_req` | out | 1 | 200,0 | `D_FF`@-480,2820 |
| `INT_vstr` | out | 1 | 220,0 | **Ground** (임시) |
| `INT_pstr` | out | 1 | 240,0 | **Ground** (임시) |
| `INT_ret` | out | 1 | 260,0 | **Ground** (임시) |

### 아래쪽: MMU / 캐시 (미구현)

| 핀 | 방향 | 비트 | 위치 | 내부 |
|---|---|---|---|---|
| `MMU_abs` | in (3-state) | 32 | 40,320 | 소비 1 |
| `MMU_limit` | out | 32 | 80,320 | **Ground**(240,3980), 넷 `PGLM` |
| `MMU_base` | out | 32 | 120,320 | **Ground**(240,3920), 넷 `PGBS` |
| `MMU_dis` | out | 1 | 160,320 | **Ground** |
| `MMU_en` | out | 1 | 200,320 | **Ground** |
| `CSH_mode` | out | 2 | 240,320 | **Ground** |
| `CSH_run` | out | 1 | 280,320 | **Ground** |

## 4. 시작 순서

- **cpu-32 / computer-32 (10-25 23:50 수정)**: `E`, `run`, `boot`를 외부 입력으로 직접 줍니다. E와 boot를 동시에 1로 주면 RS가 가득 찬 채(`RS_full`, h29) 멈춥니다. 순서는 다음과 같습니다.
  1. `E=0`, `boot=1`, `run=0`: RS 리셋 상태에서 boot 에지가 잡혀 PC = `reset-vec`가 됩니다.
  2. 몇 클럭 뒤 `E=1`, `boot=0`, `run=1`.
- dsim에서는 다음처럼 실행합니다. selftest가 seed 1, 3, zero와 seed 없이 모두 `Y end`까지 갔습니다(2026-10-26).
  ```
  java -Dset=E=0,run=0,boot=1 -Dat="10:E=1,boot=0,run=1" -cp "$J;build" Sim ../../components/computer-32.dig - ""
  ```
- **cpu-test**: 같은 세 신호를 내부 RS_FF_AS가 만듭니다. `RS_FF_AS`@-660,-1600의 Q가 `E`이고 ~Q가 `boot`(넷 `B`)입니다. `RS_FF_AS`@-660,-1540의 Q가 `run`입니다. 그래서 `-Dset=E=1,run=1`만 주면 되고, 클럭은 DipSwitch mux(-580,-1640)로 고릅니다.

## 5. 회로 영역

Rectangle 라벨 기준이고, 좌표는 cpu-32 회로 좌표입니다. 영역 안의 하위 회로만 적었습니다. 나머지는 기본 소자입니다.

### x -680..520 (왼쪽 열)

| 영역 | y | 내용 |
|---|---|---|
| decode circuit | -4180..-1980 | syscall, iret, intervec/pri 디코드 |
| IO pins | -1940..-820 | 외부 In/Out 35개, Clock |
| primeary signals | -780..-420 | 클럭 반전 `C_ct` 등 |
| pipeline controller | -380..960 | FT/DC/ISS 단계 제어. inst id manager(-640..480, 220..920): `id-manager`, `flush-mux`, DipSwitch 2개 |
| kernal mode system | 1000..1340 | `mode` |
| interrupt system | 1380..3440 | 인터럽트 상태 RS_FF들. interrupt commit(2700..3400) |
| ext system | 3480..4420 | 확장 레지스터(`ldexp`/`stexp`) |

### x 560..2680 (decode)

| 영역 | y | 내용 |
|---|---|---|
| decode circuit | -4180..-300 | 명령어 종류별 디코드: ALU, register, stack, RAM (x 600..1740), jump, call, return, wait/halt, block, exld/st (x 1780..2640) |
| register system | -260..900 | `regFile4`, ROB(`ROB-gen`, `ROB-PC`, `ROB-SC`, `ROB-FL`, `ROB-INT`) |
| program counter controller | 940..1340 | `BTB`, `add-4c32` |
| bus system | 1380..1940 | 명령어 버스 레지스터 `I_A-_R`, `I_D-_R`. DipSwitch 1개(660,1440) |
| data bus ctrl | 1980..3440 | `inst-sfcomp` ×2, 데이터 버스 Driver |
| trap | 3480..4420 | `inst-comp`, `bus16x2-demux`, `bus16-demux` |

### x 1880..3840 (실행 유닛)

| 영역 | y | 내용 |
|---|---|---|
| stack circuit | -260..120 | `addsub-4c32` (SP 갱신) |
| flag circuit | -260..120 (x 3000..) | |
| ALU system | 160..1820 | `alu-32` ×2, `inst-comp` ×4, `bus16x2-demux` ×2 |
| dir circuit | 1860..3040 | mov/set/확장 레지스터. `inst-comp` ×3 |
| load/store circuit | 3080..4420 | `load-buffer`, `store-buffer`, `add-s16c32`, `add-4c32`, `bus16-demux` ×4. DipSwitch `MB_busy`(3500,3520) |

### x 2720..4480 (issue)

| 영역 | y | 내용 |
|---|---|---|
| issue circuit | -4180..-300 | 종류별 issue 조건: ALU, register, stack, RAM, block, jump, call, return, wait/halt, exld/st, syscall, iret, intervec/pri |

### x 3880..5840 (오른쪽 열)

| 영역 | y | 내용 |
|---|---|---|
| control bus | -2220..-300 (x 4520..) | `res-station` |
| FU aribiter | -260..700 | `store-controller`, `store-controller-half` ×2, `data-select-32/8/4/8H/6H/4H` |
| jmp circuit | 740..1760 | `add-s16c32`, `bus16-demux` ×2 |
| return unit | 1800..2740 | `inst-comp` ×2 |

fetch/decode/issue 정지 신호 `halt-FT-DC-ISS`는 Or@220,500에서 만듭니다. 입력은 `id_full`, `RS_full`, `INT_HALT`, DipSwitch 2개(100,580 / 160,560), `LDB_busy`, `STB_busy`입니다.

## 6. 하위 모듈 트리

```
cpu-32.dig
- BTB.dig
- ROB-gen.dig / ROB-PC.dig / ROB-FL.dig / ROB-INT.dig
- ROB-SC.dig             - fill-16 x2 - fill-8 x2
- regFile4.dig           - mux8x4 x2, reg8x4 x5, reg8x4M x26
- id-manager.dig         - fill-16 x2 - fill-8 x2
- flush-mux.dig
- res-station.dig        - fill-8, inst-comp x9, inst-wfile (inst-comp x29)
- load-buffer.dig        - inst-comp x8
- store-buffer.dig       - inst-comp x8
- store-controller.dig   - index-compressor (add-h), priority-test8V x2
- store-controller-half.dig x2 - index-compressor-half (add-h), priority-test4V x2
- alu-32.dig x2          - LCG-32 (LCG-4 x8, LCG-4C), expand-32 x4, invert-32 x2
- add-4c32.dig x2        - LCGP-28 (LCGP-4 x7, LCGP-8C)
- addsub-4c32.dig        - LCGP-28
- add-s16c32.dig x2      - LCG-16 (LCG-4 x4, LCG-4C), LCGP-16 (LCGP-4 x4, LCGP-4C)
- data-select-32 / -8 / -4 / -8H / -6H / -4H x2
- inst-comp.dig x10, inst-sfcomp.dig x2
- bus16-demux.dig x7, bus16x2-demux.dig x5
```

- cpu-32에서 쓰는 파일은 50개이고, `cpu-test`는 여기에 debug 모듈 4개와 `ram-test`, `rom-test`를 더 씁니다.
- `bus16-demux`의 `bus`, `id-manager`의 `S1_inst`/`S2_inst`/`E_inst` 같은 핀은 3-state In입니다. 넷만 보면 구동원이 PullDown뿐인 것처럼 보이지만, 하위 회로 안의 Driver가 구동합니다(예: `V1_inst`). 넷을 분석할 때 주의해야 합니다.

## 7. 알려진 동작과 문제

- **floating 버스 latch**: `I_D-_R`의 enable이 `dc_e`뿐이라, `I_valid=0`(RAM 대기)이어도 버스를 latch합니다.
  - 부팅 중 RAM 0x1c를 fetch할 때(h99) 아무도 구동하지 않는 `I_D-_bus`를 읽습니다.
  - Digital은 high-Z를 읽을 때마다 무작위 값을 주므로, 그 값이 무효 명령어로 decode됩니다(`BUS_INSTV`=0으로 표시).
  - 10-25 23:17 수정 뒤에는 이 명령어가 issue에 영향을 주지 않아 결과가 결정적입니다. latch 자체는 남아 있습니다.
  - dsim에서는 `-Dzseed=N`으로 재현할 수 있습니다.
- `halt`가 앞선 branch가 끝나기 전에 실행되는 문제, `st .8L/.16L` 폭 문제는 CLAUDE.md "Known circuit problems"를 보세요.
- 하위 회로 안의 DipSwitch는 GUI에서 누를 수 없고, 저장된 기본값으로 고정됩니다. cpu-32에는 4개가 있습니다.
  - (100,580), (160,560): 둘 다 정지 Or 입력입니다.
  - (660,1440): bus system의 Splitter 입력입니다.
  - (3500,3520): `MB_busy`입니다.

## 8. 미완성 부분 (구동은 PullDown뿐이고 쓰는 곳도 없는 넷)

제어 버스 신호 중 아직 연결되지 않은 것들입니다. 이름만 PullDown에 붙어 있습니다.

`CT_ROB-_ST-_E`, `CT_ROB-_EX-_E`, `CT_RF-_PCR`, `CT_RF-_WM`, `CT_RF-_WS`, `CT_RF-_DM`, `CT_RF-_E`, `CT_CALL-_LD`, `CT_USCB`, `CT_BUS-_DDG`, `CT_BUS-_DDR`, `CT_BUS-_DDM`, `CT_BUS-_DAG`, `CT_BUS-_DAM`, `CT_BUS-_DAR`, `CT_BUS-_IDR`, `CT_LDB-_LD`, `CT_STB-_LD`, `CT_ILVSM`, `CT_ALU-_M2`, `kernel-inst`, `ex_nop`, `LD_addr`, `LD_id`

구동은 있지만 쓰는 곳이 없는 넷도 있습니다(일부): `CT_ISS-_HALT`, `trap`, `ISS_kernel`, `mode`, `int-proc`, `PCA`, `JPU_CALL`, `JPU_A-_ST`, `JPU_MFE`, `MB_MFE`, `CT_UBTB`, `LDB_B-_ST`, `com1_ep`, `icmk_16`, `clrs_16`, `INT_rid-_bus`, `RX_PGBSE`, `RX_PGLMM`, `RX_PGLME`.

`CT_MVR`(`Register'MVR'`의 en)와 `INT_inst`(`id-manager.E_inst`)는 PullDown만 있어서 항상 0입니다(3-state In 쪽 구동이 없다면).

## 9. 정리할 때 볼 것

1. **cpu-test와 cpu-32 맞추기**: 시작 순서(§4)와 클럭 mux가 두 파일에서 다릅니다.
   - 한쪽으로 정한 뒤 `sync-nondebug.sh`의 cpu-32/computer-32 단계를 맞춰야 합니다.
   - cpu-test 쪽을 고친다면, 핀 `E`/`run`/`boot`를 RS_FF_AS 대신 In으로 바꾸면 sync 결과가 지금의 cpu-32와 같아집니다.
   - 이 경우 `set_dip.py` 단계는 필요 없어집니다.
2. **임시 Ground 출력**: `INT_vstr`/`INT_pstr`/`INT_ret`, `MMU_*`, `CSH_*`.
3. **안 쓰는 입력**: `INT_ker`, `INT_rqac`.
4. **명령어 latch**: `I_D-_R` enable에 `I_valid`를 넣을지 정합니다(§7).
5. **어디서도 쓰지 않는 `.dig`**: 아래 파일은 cpu-32, cpu-test, computer-32 어디에서도 쓰지 않습니다.
   - 작업 중일 수 있는 파일(`IPU-test`, `MMU-BL`, `LCG-20`)은 지우면 안 됩니다.
   - `RAS.dig`(return address stack)는 CLAUDE.md에 있지만 cpu-32에는 인스턴스가 없습니다.
   ```
   BTB-buff, IPU-test, LCG-20, LCGP-32, LCGP-8, MMU-BL, RAS, ROB-gen 2, SC-buff, busMF16-demux,
   computer-32-test, data-selector, fill-4, flush-mux-old, inst-comp3, inst-min, inst-wfile-debug,
   map-inst2reg, map2-inst2reg, mux1x4, priority-16, priority-4, priority-8, priority-test8,
   priority-test8V8B, queue, reg8x4-ready, regFile, regFile2, regFile3, register32x32, register32x8,
   rename test, res-station 2, rotateL-4, rotateR-4, sub-4c32, trap-ref-old
   ```
6. **영역 이름**: 철자를 맞출 곳이 있습니다(`primeary`, `aribiter`, `kernal`).
