# test — 회로 검증용 프로그램

`program/test/`. Digital 회로(`components/cpu-test.dig`)의 버그를 좁히려고 만든 작은 프로그램들입니다. 에뮬레이터용 테스트가 아니라 회로에서 돌려 보는 용도입니다. 각 파일 맨 위 주석에 테스트 내용, 키 입력, 예상 값, 당시 결과가 자세히 있습니다.

## 빌드 / 실행

```
; program/ 에서 (예: fill)
dasm test/fill.asm -o fill.o
dlnk fill.o -o ../boot/rom.bin
```

- `fp_str2f` 등을 쓰는 테스트(`memtest8`, `selftest`)는 `dasm floatingpoint/float.asm -o float.o`를 만들어 함께 링크합니다(테스트 오브젝트를 먼저).
- 파일 주석에는 `-o run.txt`로 되어 있습니다. 예전에 `cpu-32 test.dig`(지금 `cpu-test.dig`)의 preload program file로 바로 넣던 방식입니다. 지금 회로는 boot ROM이 `boot/rom.bin`을 RAM으로 복사해서 실행하므로, `boot/rom.bin`으로 링크합니다(루트 `CLAUDE.md`의 "Running on the circuit").
- 헤드리스로 돌리려면 `tools/dsim`의 `Sim`을 씁니다(`tools/dsim/README.md`). 프로그램 인자에 `-`를 주면 ROM 파일을 그대로 씁니다.
- 대부분 결과를 터미널에 `<글자> <값>` 형식으로 출력합니다. 글자를 테스트 **전에** 출력하므로, 멈추거나 재시작하면 마지막 글자가 문제의 테스트입니다.

## 목록

| 파일 | 확인하는 것 | 입력 / 출력 |
|---|---|---|
| `fill.asm` | GraphicCard(`0x8000000`, 160×90, word 1개 = 픽셀 1개)를 `RGB(0,0,0)`(0x8000)으로 채움 | 입력 없음. 화면 전체가 칠해지면 성공 |
| `memtest.asm` | store → load 순서 | `<글자> <오류 수 hex>`(00000000 = 정상), 각 테스트를 `ROUNDS`번 반복 |
| `memtest2.asm` | memtest `a`(st 후 ld)가 멈추는 원인 좁히기 | `<테스트> <읽은 값 hex>`, 대부분 한 번씩 |
| `memtest3.asm` | 루프 안의 `st X ; ld X` 중 어느 부분이 멈추는지 | 키 1..9로 테스트 하나씩, 예상 값 00000088 |
| `memtest4.asm` | flush된 wrong-path store가 정상 경로 load에 보이는지 | 키 1..7. id-manager 점유도 함께 관찰 |
| `memtest5.asm` | 더 젊은 store가 오래된 load를 막는지, ld/st 주소 비교 비트 | 키를 누르면 8 × (ld X ; st X+OFF), `ok` |
| `memtest6.asm` | ld X 뒤에 주소가 늦게 나오는 push(M4.csv 교착) | 키 1.., 예상 합 00000088 |
| `memtest7.asm` | flush하는 분기가 id F일 때 id-manager epoch 복원 | 키 7, 8, a, b. 16라운드 중 멈추는 라운드 확인 |
| `memtest8.asm` | `fp_str2f` 안의 calc 재시작 구간(call → 짧은 ret) | 키 1..9, a..m. 결과와 SP 출력 (`float.o` 필요) |
| `selftest.asm` | calc가 쓰는 명령들이 회로에서 맞게 도는지 | `<글자> <o\|x\|?> <값 hex>` (`float.o` 필요) |
| `flushstall.asm` | store가 버스를 잡은 동안 오는 call/jmp/분기/ret flush (FT stage의 옛 fetch가 살아남는 버그, calc_nh `2*3-1`) | 입력 없음. `<케이스> PASS` / `FAIL <결과 hex>` |
| `ldstmix.asm` | 밀린 load 여러 개와 store를 섞었을 때 load 값 (bus controller mid 겹침 버그) | 입력 없음. `m1`..`m6` `PASS` / `FAIL <오류 수 hex>` |
| `rotate.asm` | `rol`/`roli`/`ror`/`rori` (X = 0x12345678, 회전량 0~36) | 입력 없음. `<케이스> <결과 hex>`, 에뮬레이터와 비교 |
| `knownbug.asm` | 루트 `CLAUDE.md`의 알려진 회로 문제: `st .8L/.16L/.8H`, 분기 바로 뒤 `halt` | 입력 없음. 고쳐질 때까지 DIFF가 정상 |
| `selfflush.asm` | 늦은 store를 앞지른 load의 self flush(`document/errors.md` 11번): 일반 load, dest = base, 의존 load, `pop`, `ret`, `st.stack` 뒤 stack 변경 | 입력 없음. `<글자> <오류 수 hex>`, 에뮬레이터와 비교. 2026-11-07: 처음엔 `g`, `j`, `l` FAIL(`stack` base store가 더 젊은 pop/ret/push가 바꾼 stack을 씀). `st.stack`이 PCA를 쓰도록 고친 뒤 모두 PASS(zseed 1~3) |
| `haltbr.asm` | 늦게 풀리는 not-taken 예측 분기 바로 뒤 `halt` (H1..H10) | 이미지마다 `#define H<n> 1`을 앞에 붙여 빌드. `AB`(H6은 `A123`)가 정상. 2026-11-07: 10개 × zseed 1~3 모두 정상 |
| `int_*.asm` (`int.hsm`) | 인터럽트 진입, 복귀, `syscall`, `interst`/`interld`, 확장 레지스터 | 입력 없음. `tools/dsim/inttest.py`로 실행. 아래 "인터럽트 회귀 테스트" |

`M4.o`, `M7.o`, `fill.o`는 예전 빌드 결과입니다.

## 회귀 테스트 (`flushstall`, `ldstmix`, `knownbug`)

`tools/dsim/runtests.sh`에 들어 있습니다. 에뮬레이터 출력과 비교하므로, DIFF가 나면 다른 줄의 케이스 이름이 문제의 케이스입니다. 셋 다 한 번 돌리는 데 6만 half 이하입니다.

```
bash tools/dsim/runtests.sh flushstall ldstmix knownbug     ; 루트에서. 결과: program/build/dsim/<이름>.sim / .emu
```

- `flushstall`: 케이스 이름은 `<종류><store 수 K><nop 수 N>`입니다. K개의 store와 N개의 nop 뒤에 flush하는 명령(`c` call, `r` store 뒤 ret하는 callee, `d` call 두 번 연속, `p` push 두 번 뒤 call, `j` jmp.one, `b` cmp 뒤 taken jmp.zero)을 두고, 그 뒤 word를 모두 `addi gen[7], 1`(ret 뒤는 `+0x100`)로 채웁니다. 결과는 `gen[7] << 16 | gen[6] << 8 | (스택 차이)`이고, call 종류는 `00040100`(`d`는 `00040200`), jmp/분기 종류는 `00000100`이 정상입니다. 옛 fetch가 실행되면 `gen[7]`이 늘어납니다.
- `ldstmix`: `A[i] = 0x5A000011 + i*0x01030507`을 쓰고, 16라운드 동안 load 8개 연속(m1), ld/st 교대(m2), st 뒤 같은 주소 ld(m3), load한 값을 store하고 다시 load(m4), 그 결과 확인(m5), store를 끼운 pointer chase(m6)를 하며 load 값을 모두 확인합니다.
- `knownbug`: `0x11223344`에 `st .8L`/`.16L`/`.8H`를 하고 word를 읽습니다(k1..k3). 마지막은 3바퀴 루프의 분기 바로 뒤 `halt`이고 `h 123`이 나와야 합니다. 이 테스트만 `halt`로 끝납니다.

### 결과 (2026-11-05, dsim, `cpu-test.dig` 작업 트리)

- `flushstall` SAME (모두 PASS). 처음에는 `c10`, `c30`~`c33`이 `FAIL 00050100`이었고, 프론트엔드 수정 뒤 통과했습니다(`document/errors.md` 15번).
- `ldstmix` SAME (모두 PASS). store 응답을 다시 켜면 확인할 것.
- `knownbug` DIFF: k1..k3 FAIL(`000000AA`, `0000BEEF`, `000000CC`: 직후 load가 폭을 무시한 forward 값을 받음, 메모리는 맞음. `document/errors.md` 17번). `h 123`은 맞게 나옵니다. 분기 뒤 `halt` 문제는 이 경우에는 재현되지 않습니다.

## 인터럽트 회귀 테스트 (`int_*.asm`)

에뮬레이터에 인터럽트가 없으므로 기대값은 `document/ISA.md` 2.2절로 계산합니다.

`tools/dsim/inttest.py`로 `components/computer-32-test.dig`(CPU + IPU)에서 돌립니다(2026-11-15부터. 그 전의 `runint.py`는 IPU 없는 `cpu-test.dig`에 `INT_in`/`INT_vec`/`INT_lev`를 직접 넣었고, 지웠습니다).

```
python tools/dsim/inttest.py [-j N] [-seeds 1,2,3] [-nobuild] [그룹...]     ; 루트에서. 그룹: ext prec sled misp iret sys tbl
```

- 인터럽트는 IPU 입력선 `IRQ`(= 3, top-level In `I-in`의 bit 3)에 2 half 펄스를 넣어 만듭니다. IPU는 1 half 펄스도 잡습니다. 들어가는 프로그램은 `INTVEC()`(`int.hsm`)로 `int_vec[IRQ] = handler`, `int_pri[IRQ] = 1`을 씁니다. `int_pri`가 0인 선은 받지 않습니다.
- 시작은 boot-sequencer(`-Dset=S=0,R=1,B_used=0,I-in=0 -Dat=6:R=0;10:S=1;16:S=0`)이고, 프로그램은 `-Drom`으로 boot ROM이 RAM에 복사합니다.
- 인터럽트 없이 먼저 돌려 PC가 루프 주소에 닿는 half H를 찾고, 거기서부터 2 half 간격으로 펄스를 넣습니다. `I-in`이 오른 뒤 약 42 half에 IPU의 `acp`가 1 half 나오고, 그 4 half 뒤 PC가 핸들러가 됩니다.
- 결과는 `PASS`, `FAIL`, `XFAIL`(지금 실패가 예상됨, 이유는 스크립트의 `XFAIL`), `XPASS`(예상과 달리 통과), `SKIP`(이번 실행에서 판단 불가)입니다. `FAIL`이나 `XPASS`가 있으면 종료 코드 1이고, `XPASS`가 나오면 `XFAIL`에서 그 항목을 지웁니다.
- `components/`를 `program/build/dsim/int/components/`로 복사해 돌리므로 실행 중에 회로를 저장해도 됩니다. 출력은 같은 폴더의 `<프로그램>-z<seed>-<H>.sim`, `.delta`입니다. 전체 약 2분입니다.

| 항목 | 프로그램 | 확인하는 것 |
|---|---|---|
| 1a, 1b | `int_ext` | `stexp`/`ldexp` 왕복. 1a: `istack`, `panicvec`, `intlevel`(5비트), `cpuid`. 1b: `pbase`, `plimit` |
| 2 | `int_prec` | 루프 두 주기의 모든 위상에서 진입해 핸들러가 끝까지 도는지 |
| 3a, 3b, 3c | `int_prec` | 점 개수와 `gen[5]`가 한 commit 지점인지(3a), 복귀 PC까지 맞는지(3b), 복귀 PC +4를 허용하고 맞는지(3c) |
| 4 | `int_prec` | `istack -= 4; mem[istack] = 복귀 PC`, 원래 `istack` 칸은 그대로 |
| 5 | `int_sled` | 복귀 PC = 첫 미commit 명령. `addi` 연속이라 `sled + 4*gen[5]`로 정확히 계산됩니다 |
| 6a, 6b | `int_prec`, `int_sys` | `syscall`이 없으면 `INT_req`가 0, 있으면 1 |
| 7a, 7b, 7c | `int_iret` | `iret` 뒤 프로그램이 이어지는지, `istack`과 `int-level`이 복원되는지 |
| 8a, 8b | `int_sys` | `syscall`이 핸들러로 들어가고 복귀 PC가 다음 명령인지, 돌아와 `BC`를 출력하는지. IRQ 펄스 없이 IPU가 `INT_req`에 응답해야 합니다 |
| 9 | `int_tbl` | `interst`/`interld` 왕복(`int_vec[3]`, `int_pri[3]`). 출력 `A00000105 00000009 E` |
| 10a, 10b, 10c | `int_misp` | 번갈아 오분기하는 분기 주변 진입. 복귀 PC와 `gen[5..7]`이 정상 경로의 명령 경계인지(10a는 +4 허용, 10b는 정확히), 끝까지 도는지(10c) |
| 11 | `int_misp` | Fl_INT와 Fl_JPU가 같은 half인 실행의 결과. 그런 실행이 없으면 `SKIP` |
| 13 | `int_prec` | 핸들러 안의 `int-level`이 `{mask 1, level 1}` = 0x11 |

- 모든 실행에 `cpu-32`의 tunnel `PCS`(PC), `com1`, `com2`, `INT_req`, `Fl_INT`, `Fl_JPU` probe를 붙입니다(`-Dtap`).
- 위상 sweep은 2 half 간격의 짝수 half(클럭 상승)만 봅니다.

### 결과 (2026-11-15, `inttest.py`, `computer-32-test.dig` 00:12 저장본, `prec`는 zseed 1, 2, 3, 나머지는 1)

| 결과 | 항목 |
|---|---|
| PASS | 1a, 1b, 2, 3a, 3b, 3c, 4, 5, 6a, 6b, 9, 10a, 10b, 10c, 13 |
| XFAIL | 7a, 8a, 8b |
| SKIP | 7b, 7c (`iret`이 돌아오지 않음), 11 (38 위상 중 Fl_INT와 Fl_JPU가 겹친 실행 없음) |

- 7a: 8회 모두 `Ii` 출력 뒤 PC=0x1C(`iret`)에서 stall. iret 경로가 아직 없습니다.
- 8a, 8b: `INT_req`는 half 4624에 오르지만 IPU가 응답하지 않아 `L00000278 A` 뒤 PC=0x278에서 stall.
- 예전 bench(아래)에서 실패하던 2, 3b, 5, 10a~10c가 통과합니다. 복귀 PC +4 문제(5)는 이 회로에서는 나오지 않습니다(16회 모두 정확).

### 예전 결과 (2026-11-06, `runint.py`, `cpu-test.dig` 작업 트리 22:19 저장본)

| 결과 | 항목 |
|---|---|
| PASS | 1a, 3a, 3c, 4, 6a, 6b, 13 |
| XFAIL | 1b(`pbase`/`plimit`이 0으로 읽힘, Ground 연결), 2, 3b, 5, 7a, 8a, 8b, 9, 10a, 10b, 10c, 11 |
| SKIP | 7b, 7c (`iret`이 돌아오지 않아 판단 불가) |

- 2: 48회 중 12회 stall. seed마다 같은 위상 4곳(H=12766, 12772, 12782, 12788), PC=0x3C/0x38.
- 5: 16회 모두 복귀 PC가 정답 +4. 3b가 실패하는 것도 이 때문입니다(3c는 통과).
- 7a: `Ii` 출력 뒤 PC=0x20에서 stall. `INT_ret`은 Ground입니다.
- 10a: 32회 중 7회. 예: H=24642, `gen[5]`=180(짝수 바퀴, 정상 경로는 +28)인데 복귀 PC가 오분기 경로의 +24입니다. Fl_INT 뒤 12 half에 Fl_JPU가 또 나옵니다.
- 10c: 38회 중 6회 stall. H=24614, 24678은 항목 2와 같은 모양이고, H=24656~24662(Fl_JPU 직후)는 핸들러의 `I`도 출력하지 않고 PC=0x904, 0x8E0~0x8E8에서 멈춥니다.
- 11: H=24654 한 번 발생. 오분기 경로의 `addi gen[6]`이 반영되어 `gen[6]`=91입니다(정답 90).

## fill.asm 상태 (2026-10-17, dsim)

- 에뮬레이터에서는 14400픽셀을 다 칠하고 끝납니다.
- 회로에서는 **픽셀 1개(주소 0)만 칠하고 멈춥니다.** 루프 끝 `jmp.neg` 바로 뒤의 `halt`를 회로가 분기가 결정되기 전에 실행해 버리기 때문입니다(루트 `CLAUDE.md`의 알려진 회로 문제).
- `halt`를 `jmp.one pc.32B, 0`으로 바꾸면 14400픽셀(주소 0~0x383F)이 모두 칠해지는 것을 확인했습니다.
- `GR_SIZE`는 `90*160*4`바이트입니다. `boot/src/boot.asm` 주석의 "display 160 x 80"과 다르지만, 회로의 GraphicCard 설정은 높이 90입니다.
