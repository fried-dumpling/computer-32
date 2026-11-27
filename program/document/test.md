# test — 회로 검증용 프로그램

`program/test/`. 파일이 많은 묶음은 하위 폴더에 있습니다: `interrupt/`(`int.hsm`, `int_*.asm`), `memory/`(`memtest*.asm`, `ldstmix.asm`, `selfflush.asm`). 나머지는 `program/test/` 바로 아래에 있습니다. Digital 회로(`components/cpu-test.dig`)의 버그를 좁히려고 만든 작은 프로그램들입니다. 에뮬레이터용 테스트가 아니라 회로에서 돌려 보는 용도입니다. 각 파일 맨 위 주석에 테스트 내용, 키 입력, 예상 값, 당시 결과가 자세히 있습니다.

## 빌드 / 실행

```
; program/ 에서 (예: fill)
dasm test/fill.asm -o fill.o
dlnk fill.o -o ../boot/rom.bin
```

- `fp_str2f` 등을 쓰는 테스트(`memtest8`, `selftest`)는 `dasm floatingpoint/float.asm -o float.o`를 만들어 함께 링크합니다(테스트 오브젝트를 먼저).
- 파일 주석에는 `-o run.txt`로 되어 있습니다. 예전에 `cpu-32 test.dig`(지금 `legacy/cpu-test.dig`)의 preload program file로 바로 넣던 방식입니다. 지금 회로는 boot ROM이 `boot/rom.bin`을 RAM으로 복사해서 실행하므로, `boot/rom.bin`으로 링크합니다(루트 `CLAUDE.md`의 "Running on the circuit").
- 헤드리스로 돌리려면 `tools/dsim`의 `Sim`을 씁니다(`tools/dsim/README.md`). 프로그램 인자에 `-`를 주면 ROM 파일을 그대로 씁니다.
- 대부분 결과를 터미널에 `<글자> <값>` 형식으로 출력합니다. 글자를 테스트 **전에** 출력하므로, 멈추거나 재시작하면 마지막 글자가 문제의 테스트입니다.

## 목록

| 파일 | 확인하는 것 | 입력 / 출력 |
|---|---|---|
| `fill.asm` | GraphicCard(`0x8000000`, 160×90, word 1개 = 픽셀 1개)를 `RGB(0,0,0)`(0x8000)으로 채움 | 입력 없음. 화면 전체가 칠해지면 성공 |
| `memory/memtest.asm` | store → load 순서 | `<글자> <오류 수 hex>`(00000000 = 정상), 각 테스트를 `ROUNDS`번 반복 |
| `memory/memtest2.asm` | memtest `a`(st 후 ld)가 멈추는 원인 좁히기 | `<테스트> <읽은 값 hex>`, 대부분 한 번씩 |
| `memory/memtest3.asm` | 루프 안의 `st X ; ld X` 중 어느 부분이 멈추는지 | 키 1..9로 테스트 하나씩, 예상 값 00000088 |
| `memory/memtest4.asm` | flush된 wrong-path store가 정상 경로 load에 보이는지 | 키 1..7. id-manager 점유도 함께 관찰 |
| `memory/memtest5.asm` | 더 젊은 store가 오래된 load를 막는지, ld/st 주소 비교 비트 | 키를 누르면 8 × (ld X ; st X+OFF), `ok` |
| `memory/memtest6.asm` | ld X 뒤에 주소가 늦게 나오는 push(M4.csv 교착) | 키 1.., 예상 합 00000088 |
| `memory/memtest7.asm` | flush하는 분기가 id F일 때 id-manager epoch 복원 | 키 7, 8, a, b. 16라운드 중 멈추는 라운드 확인 |
| `memory/memtest8.asm` | `fp_str2f` 안의 calc 재시작 구간(call → 짧은 ret) | 키 1..9, a..m. 결과와 SP 출력 (`float.o` 필요) |
| `selftest.asm` | calc가 쓰는 명령들이 회로에서 맞게 도는지 | `<글자> <o\|x\|?> <값 hex>` (`float.o` 필요) |
| `flushstall.asm` | store가 버스를 잡은 동안 오는 call/jmp/분기/ret flush (FT stage의 옛 fetch가 살아남는 버그, calc_nh `2*3-1`) | 입력 없음. `<케이스> PASS` / `FAIL <결과 hex>` |
| `memory/ldstmix.asm` | 밀린 load 여러 개와 store를 섞었을 때 load 값 (bus controller mid 겹침 버그) | 입력 없음. `m1`..`m6` `PASS` / `FAIL <오류 수 hex>` |
| `rotate.asm` | `rol`/`roli`/`ror`/`rori` (X = 0x12345678, 회전량 0~36) | 입력 없음. `<케이스> <결과 hex>`, 에뮬레이터와 비교 |
| `knownbug.asm` | 예전 `CLAUDE.md`의 알려진 회로 문제: `st .8L/.16L/.8H` 직후 word load(`document/errors.md` 17번), 분기 바로 뒤 `halt` | 입력 없음. 에뮬레이터와 비교. 2026-11-27 폭 수정 뒤 SAME |
| `memory/ststld.asm` | 같은 word에 store 2개 이상 뒤 load: 가장 젊은 older store 값을 받는지(a..h, 늦은 data, 다른 주소 store 사이) | 입력 없음. `<케이스> PASS` / `FAIL <읽은 값 hex>`, 에뮬레이터와 비교 |
| `memory/straddle.asm` | store buffer에 있는 store에서 같은 word의 다른 byte offset으로 forward(a..n). 정렬된 접근만(비정렬은 fault 5, `int_align`) | 입력 없음. `<케이스> PASS` / `FAIL <읽은 값 hex>`, 에뮬레이터와 비교. 2026-11-27 모두 PASS(errors.md 57) |
| `memory/selfflush.asm` | 늦은 store를 앞지른 load의 self flush(`document/errors.md` 11번): 일반 load, dest = base, 의존 load, `pop`, `ret`, `st.stack` 뒤 stack 변경 | 입력 없음. `<글자> <오류 수 hex>`, 에뮬레이터와 비교. 2026-11-07: 처음엔 `g`, `j`, `l` FAIL(`stack` base store가 더 젊은 pop/ret/push가 바꾼 stack을 씀). `st.stack`이 PCA를 쓰도록 고친 뒤 모두 PASS(zseed 1~3) |
| `haltbr.asm` | 늦게 풀리는 not-taken 예측 분기 바로 뒤 `halt` (H1..H10) | 이미지마다 `#define H<n> 1`을 앞에 붙여 빌드. `AB`(H6은 `A123`)가 정상. 2026-11-07: 10개 × zseed 1~3 모두 정상 |
| `interrupt/int_*.asm` (`interrupt/int.hsm`) | 인터럽트 진입, 복귀, `syscall`, `interst`/`interld`, 확장 레지스터 | 입력 없음. `tools/dsim/inttest.py`로 실행. 아래 "인터럽트 회귀 테스트" |

`M4.o`, `M7.o`, `fill.o`, `memtest2.o`, `selftest.o`는 예전 빌드 결과입니다(커밋 안 됨). 하위 폴더의 파일은 `dasm test/memory/memtest.asm -o memtest.o`처럼 빌드합니다.

## 회귀 테스트 (`flushstall`, `ldstmix`, `knownbug`, `ststld`, `selfflush`, `straddle`)

`tools/dsim/runtests.sh`에 들어 있습니다. 에뮬레이터 출력과 비교하므로, DIFF가 나면 다른 줄의 케이스 이름이 문제의 케이스입니다. 셋 다 한 번 돌리는 데 6만 half 이하입니다.

```
bash tools/dsim/runtests.sh flushstall ldstmix knownbug ststld selfflush straddle     ; 루트에서. 결과: program/build/dsim/<이름>.sim / .emu
```

- `flushstall`: 케이스 이름은 `<종류><store 수 K><nop 수 N>`입니다. K개의 store와 N개의 nop 뒤에 flush하는 명령(`c` call, `r` store 뒤 ret하는 callee, `d` call 두 번 연속, `p` push 두 번 뒤 call, `j` jmp.one, `b` cmp 뒤 taken jmp.zero)을 두고, 그 뒤 word를 모두 `addi gen[7], 1`(ret 뒤는 `+0x100`)로 채웁니다. 결과는 `gen[7] << 16 | gen[6] << 8 | (스택 차이)`이고, call 종류는 `00040100`(`d`는 `00040200`), jmp/분기 종류는 `00000100`이 정상입니다. 옛 fetch가 실행되면 `gen[7]`이 늘어납니다.
- `ldstmix`: `A[i] = 0x5A000011 + i*0x01030507`을 쓰고, 16라운드 동안 load 8개 연속(m1), ld/st 교대(m2), st 뒤 같은 주소 ld(m3), load한 값을 store하고 다시 load(m4), 그 결과 확인(m5), store를 끼운 pointer chase(m6)를 하며 load 값을 모두 확인합니다.
- `ststld`: a..h 모두 `DELAY`(느린 load와 addi 사슬)로 commit을 늦춰 store가 store buffer에 남은 상태에서 load합니다. e/f는 store data가 늦게 옵니다(load-buffer의 늦은 store flush, ROB-SC self flush stack 복구도 같이 탐).
- `selfflush`: 위 표 참고.
- `straddle`: X = 0x11223344, X+4 = 0x55667788로 시작합니다. a..h는 store보다 좁은 load가 다른 offset에서 읽는 경우(`st.32B X` 뒤 `ld.8L X+1/+2/+3`, `ld.16L X+2`, `ld.8H X+1`, `st.16L X+2` 뒤 `ld.8L X+3/+2`, `st.8H X+2` 뒤 `ld.8L X+2`), i..k는 store보다 넓은 load(store data와 메모리를 합침), l은 같은 offset, m은 다음 word store와 정렬 load(ldstmix m4 패턴), n은 d에서 DELAY 없음. 정렬 규칙(half는 짝수, word는 4의 배수 주소, byte는 어디나)에 맞는 접근만 씁니다. word 경계에 걸친 예전 케이스(u1..ua)는 fault 5가 나므로 뺐습니다(2026-11-27).
- `knownbug`: `0x11223344`에 `st .8L`/`.16L`/`.8H`를 하고 word를 읽습니다(k1..k3). 마지막은 3바퀴 루프의 분기 바로 뒤 `halt`이고 `h 123`이 나와야 합니다. `halt`로 끝납니다. STW는 값을 PUTC 전에 준비합니다(store가 늦으면 다음 load가 self flush되어 forward 버그가 가려짐, 2026-11-27).

### 결과 (2026-11-05, dsim, `cpu-test.dig` 작업 트리)

- `flushstall` SAME (모두 PASS). 처음에는 `c10`, `c30`~`c33`이 `FAIL 00050100`이었고, 프론트엔드 수정 뒤 통과했습니다(`document/errors.md` 15번).
- `ldstmix` SAME (모두 PASS). store 응답을 다시 켜면 확인할 것.
- `knownbug` DIFF: k1..k3 FAIL(`000000AA`, `0000BEEF`, `000000CC`: 직후 load가 폭을 무시한 forward 값을 받음, 메모리는 맞음. `document/errors.md` 17번). `h 123`은 맞게 나옵니다. 분기 뒤 `halt` 문제는 이 경우에는 재현되지 않습니다.

## 인터럽트 회귀 테스트 (`int_*.asm`)

에뮬레이터에 인터럽트가 없으므로 기대값은 `document/ISA.md` 2.2절로 계산합니다.

`tools/dsim/inttest.py`로 `components/computer-32-test.dig`(CPU + IPU)에서 돌립니다(2026-11-15부터. 그 전의 `runint.py`는 IPU 없는 `cpu-test.dig`에 `INT_in`/`INT_vec`/`INT_lev`를 직접 넣었고, 지웠습니다).

```
python tools/dsim/inttest.py [-j N] [-seeds 1,2,3] [-nobuild] [-snap] [그룹...]     ; 루트에서. 그룹: ext prec sled misp flc iret iret2 iretpre sys sysprev sysirq nest wpsys sysmem user sys0 tbl
```

- `-snap`: `components/`를 다시 복사하지 않고 지난 실행의 복사본(`program/build/dsim/int/components/`)을 씁니다. 회로를 고치는 중에 같은 회로로 다시 돌릴 때 씁니다.

- 인터럽트는 IPU 입력선 `IRQ`(= 3, top-level In `I-in`의 bit 3)에 2 half 펄스를 넣어 만듭니다. `computer-32-test.dig`의 장치 latch `RS-64`(S = `I-in`, R = `I-acp`, Q = `I-act`)가 그 선을 acp가 올 때까지 high로 잡아 IPU에 줍니다(11-19 규칙: IRQ 선은 acp까지 high, `errors.md` 35, 38번). `I-act`가 이미 1이거나 acp가 오는 edge에 들어온 펄스는 무시될 수 있습니다. 들어가는 프로그램은 `INTVEC()`(`int.hsm`)로 `int_vec[IRQ] = handler`, `int_pri[IRQ] = 1`을 씁니다. `int_pri`가 0인 선은 받지 않습니다.
- 시작은 boot-sequencer(`-Dset=S=0,R=1,B_used=0,I-in=0 -Dat=6:R=0;10:S=1;16:S=0`)이고, 프로그램은 `-Drom`으로 boot ROM이 RAM에 복사합니다.
- 인터럽트 없이 먼저 돌려 PC가 루프 주소에 닿는 half H를 찾고, 거기서부터 2 half 간격으로 펄스를 넣습니다. `I-in`이 오른 뒤 약 42 half에 IPU의 `acp`가 1 half 나오고, 그 4 half 뒤 PC가 핸들러가 됩니다.
- 결과는 `PASS`, `FAIL`, `XFAIL`(지금 실패가 예상됨, 이유는 스크립트의 `XFAIL`), `XPASS`(예상과 달리 통과), `SKIP`(이번 실행에서 판단 불가)입니다. `FAIL`이나 `XPASS`가 있으면 종료 코드 1이고, `XPASS`가 나오면 `XFAIL`에서 그 항목을 지웁니다.
- `components/`를 `program/build/dsim/int/components/`로 복사해 돌리므로 실행 중에 회로를 저장해도 됩니다. 출력은 같은 폴더의 `<프로그램>-z<seed>-<H>b<bit>.sim`, `.delta`입니다. 전체 1,079회, `-j 6`으로 약 4분입니다(2026-11-19; 예전 방식 약 1,650회는 30분). `sysirq`, `nest`, `iret2`는 이벤트 근처만 촘촘히 돌고, 다시 시작하는 실행은 일찍 끝냅니다(`tools/dsim/README.md`). 예전처럼 모든 위치를 보려면 `-full -nolimit`를 줍니다.

| 항목 | 프로그램 | 확인하는 것 |
|---|---|---|
| 1a, 1b | `int_ext` | `stexp`/`ldexp` 왕복. 1a: `istack`, `intlevel`, `inttmplv`(lev b0-3, mask b4, ker b7만 저장: 0x5698 → 0x98, 0x5687 → 0x87), `cpuid`가 두 번 같은지. 1b: `pbase`, `plimit` |
| 2 | `int_prec` | 루프 두 주기의 모든 위상에서 진입해 핸들러가 끝까지 도는지 |
| 3a, 3b, 3c | `int_prec` | 점 개수와 `gen[5]`가 한 commit 지점인지(3a), 복귀 PC까지 맞는지(3b), 복귀 PC +4를 허용하고 맞는지(3c) |
| 4 | `int_prec` | `istack -= 4; mem[istack] = 복귀 PC`, 원래 `istack` 칸은 그대로 |
| 5 | `int_sled` | 복귀 PC = 첫 미commit 명령. `addi` 연속이라 `sled + 4*gen[5]`로 정확히 계산됩니다 |
| 6a, 6b | `int_prec`, `int_sys` | `syscall`이 없으면 `INT_req`가 0, 있으면 1 |
| 7a, 7b, 7c | `int_iret` | `iret` 뒤 프로그램이 이어지는지, `istack`과 `int-level`(INIT의 0x80)이 복원되는지 |
| 8a, 8b | `int_sys` | `syscall`이 핸들러로 들어가고 복귀 PC가 다음 명령인지, 돌아와 `BC`를 출력하는지. IRQ 펄스 없이 IPU가 `INT_req`에 응답해야 합니다. `syscall`은 id 1이므로 프로그램이 `int_vec[1]` = handler, `int_pri[1]` = 1을 씁니다(level 0이면 진입하지 않음). 출력 `L00000284 AH00000284 hBCE` |
| 9 | `int_tbl` | `interst`/`interld` 왕복(`int_vec[3]`, `int_pri[3]`). 출력 `A00000105 00000009 E` |
| 10a, 10b, 10c | `int_misp` | 번갈아 오분기하는 분기 주변 진입. 복귀 PC와 `gen[5..7]`이 정상 경로의 명령 경계인지(10a는 +4 허용, 10b는 정확히), 끝까지 도는지(10c) |
| 10d, 10e, 10f | `int_flc` | 10a~10c와 같은 검사. 분기 조건이 `st`/`ld`를 거쳐 늦게 결정됩니다(원래 Fl_INT와 Fl_JPU를 겹치게 하려는 것. Fl_INT는 없앨 예정이라 11번 항목은 11-19에 뺐습니다). 루프 150회, zseed 전부 |
| 13 | `int_prec` | 핸들러 안의 `int-level`이 `{ker 1, mask 1, level 1}` = 0x91 |
| 14a | `int_sysprev` | `syscall` 앞 명령 12종(`st`, `set`, `addi`, `nop`, 오분기된 루프 끝 등)마다 복귀 PC가 다음 명령인지. `NPAD` 0~23(`.data` 길이 → boot 복사 시간)으로 24번 빌드합니다 |
| 14b | `int_sysirq` | `syscall` 핸들러 안의 `int-level`이 `{ker 1, mask 1, int_pri[1] = 1}` = 0x91 |
| 14c, 14d | `int_sysprev` | 모든 `NPAD`에서 끝까지 도는지(14c), 끝에서 `istack` = ISTACK, `stack` = 0x3000인지(14d) |
| 15a~15d | `int_sysirq` | `syscall`(id 1)과 IRQ(id 3)가 둘 다 level 1. `syscall` 앞 루프부터 핸들러 뒤까지 모든 위상에 IRQ 펄스. 끝까지 도는지(15a), 두 핸들러가 한 번씩 도는지 = IRQ가 없어지지 않는지(15b), IRQ가 `syscall` commit 전(R ≤ sc)이거나 `syscall` 핸들러의 `iret` 뒤에 들어가고 `syscall` 복귀 PC가 다음 명령인지(15c), IRQ 핸들러 level 0x91(15d). zseed 전부 |
| 16a, 16b | `int_nest` | 낮은 IRQ(id 3, level 1) 핸들러가 `int-tmplv`를 저장하고 mask를 내린 동안(0x81) 높은 IRQ(id 4 = `I-in` bit 4, level 2)를 모든 위상에 넣습니다. 끝까지 도는지(16a). 높은 쪽이 mask 0 구간 안에서만 중첩되고(아니면 낮은 쪽 `iret` 뒤 main에서), level 0x12 / `int-tmplv` 0x01, 복원한 `int-tmplv`, 마지막 `istack`/`int-level`/`gen[5]`이 맞는지(16b). zseed 전부 |
| 17a, 17b | `int_iret2` | 첫 IRQ의 핸들러 진입부터 `iret` 뒤까지 모든 위상에 둘째 IRQ. 끝까지 도는지(17a), 진입이 정확히 2번이고 두 복귀 PC가 main에 있는지, `istack`/`int-level`/`gen[5]`(17b). 둘째 펄스를 잡는 edge 직전 half(H2−1)에 그 선의 `I-act`나 `I-acp`가 1이면 장치 latch가 펄스를 무시할 수 있어서(S=R=1, seed에 따라 다름) 1번 또는 2번을 허용합니다. zseed 전부 |
| 18 | `int_wpsys` | 항상 taken인 분기(조건이 load라 늦게 결정, BTB miss는 not taken 예측)의 fall-through에 `syscall`. `INT_req`가 0, 핸들러 진입 없음. 오경로에서 `syscall`을 fetch하지 않았으면 `SKIP` |
| 19a, 19b, 19c | `int_sysmem` | `st` 바로 뒤, `ld` 바로 뒤 `syscall`. 복귀 PC(19a), 핸들러에서 본 메모리와 `gen[6]`: 앞 `st`/`ld`는 끝나고 뒤 `st`는 안 됨(19b), `syscall` 뒤 `ld`가 핸들러의 store를 보는지(19c) |
| 20a, 20b | `int_user` | `pbase` = 0, `plimit` = 0x5000(리셋 값이면 첫 user fetch가 fault)으로 두고 `iret`(`int-tmplv` = 0)으로 user에 들어감. user 코드는 메모리에 접근하지 않음(user mode는 MMU 때문에 터미널 0x4000000에 쓸 수 없음): `gen[7]`에 표식을 넣고 `syscall`하면 kernel 핸들러가 `S<표식>`을 출력하고 로그를 남김. 출력 `L<u1> <u2> KSUSwE G...g`. user `syscall`이 들어가고(R = 다음 word) user로 돌아오는지(20a, 둘째 `syscall`로 확인), 핸들러에서 `int-level` ker = 1, `int-tmplv` ker = 0(20b). user의 `stexp`(예전 20c, id 0 가정)는 23c(privilege fault id 4)로 옮김(2026-11-25) |
| 21a, 21b | `int_sys0` | `int_pri[1]` = 0인 `syscall`: 핸들러가 돌지 않는지(21a), id 0으로 한 번 진입하고(복귀 PC = `syscall` 다음 word, `int-level` 0x9f) `iret` 뒤 끝까지 도는지(21b, 11-19 결정, `ISA.md` 2.2 "고정 id"). 프로그램이 `int_vec[0]`에 handler를 둡니다. 출력 `APBEG<R> 0000009F g` |
| 22 | `int_iretpre` | 핸들러 끝(`iret` 바로 앞) 8종(`st`, `nop`, `set`, `nop` 4개, `ld`, `ld ld` 등)을 따로 빌드(`C0`~`C7`). `syscall`과 IRQ로 한 번씩 들어가 둘 다 돌아오는지 |
| 24a~24f | `int_align` | alignment fault(id 5). kernel에서 X = 0x3800에 0x11223344, 0x55667788을 두고 c1 `ld.32B X+2`, c2 `ld.16L X+1`, c3 `st.32B X+2`, c4 `st.16L X+3`(word 경계에 걸침). 케이스 전에 `gen[2]` = SENT. 핸들러는 `A`를 출력하고 `{0xF5, R, gen[2]}`를 로그에 남긴 뒤 다음 케이스로 `iret`. 24a~24d: 케이스마다 그 케이스만 도는 변형(`FSTART s<n>`, `FONE`)으로 R = fault 낸 명령, `gen[2]`가 그대로(ld가 쓰지 않음), 메모리가 그대로(st가 쓰지 않음)인지 판정. 24e(변형 `wp`): 정렬된 `ld.16L X+2`, `ld.8L X+3`, `ld.8H X+1`이 fault 없이 맞게 읽고, 오경로(`jmp` 뒤)의 비정렬 `ld`/`st`가 trap하지 않는지. 24f: 전체 한 번에(`KAAAA`, 기록 4개). 2026-11-27 모두 PASS |
| 23a~23h | `int_fault` | page fault(id 3, `P`)와 privilege fault(id 4, `V`). `pbase` = 0, `plimit` = 0x5000, user에서 c1 `ld` 0x6000, c2 `st` 0x6000, c3 `stexp istack`, c4 `jmp` 0x6000(fetch fault), c5 `iret`, c6 터미널 `st`. 핸들러는 `{0xF3/0xF4, R, int-level, int-tmplv, istack}`을 로그에 남기고 다음 케이스로 `iret`. 케이스마다 그 케이스만 도는 변형(`FSTART s<n>`, `FONE`)으로 23a~23f를 판정(기대: R = fault 낸 명령, fetch fault는 0x6000, `int-level` 0x91, `int-tmplv` ker 0, istack 0x1FFC). 23g: 오경로(BTB miss `jmp` 뒤)의 `ld` 0x6000, `stexp`가 trap하지 않는지(변형 `wp`). 23h: 전체 한 번에(`KPPVPVPSE`, 기록 6개). 20c(id 0 가정)는 23c로 대신함. syscall/iret 중 data fault는 아직 회로에서 처리하지 않아 시험하지 않음(2026-11-23) |
| (공통) | `int.hsm` | CPU는 kernel, mask 1(`int-level` 0x90)로 시작하므로 `INIT`이 `int-level` = 0x80(ker 1, mask 0, level 0)을 씁니다(2026-11-22). `int_pri`에는 ker 비트를 함께 써야(`PKER` + level, 예: 0x81) 핸들러가 kernel로 돌고 핸들러 안 `int-level`이 0x91이 됩니다. `int_sys0`의 level 0과 `int_tbl`의 왕복 값은 그대로입니다. 핸들러는 `LOGK`/`LOGRET`/`LOGEXT`로 값을 메모리 로그(`LOG` 0x2800)에 붙입니다. ALU를 쓰지 않아 끼어든 코드의 flag가 그대로입니다. main이 끝에 `DUMPLOG`로 `G<값> ... g`를 출력합니다 |

- 모든 실행에 `cpu-32`의 tunnel `PCS`(PC), `com1`, `com2`, `INT_req` probe를 붙입니다(`-Dtap`). 17b는 top-level probe `I-act`, `I-acp`도 기록합니다.
- 위상 sweep은 2 half 간격의 짝수 half(클럭 상승)만 봅니다.

### 결과 (2026-11-25, `cpu-32.dig` 00:36 저장본, `ROB-INT.dig` 00:35, `computer-32-test.dig` 11-24 22:48, kernel 브랜치)

PASS 52, FAIL 0, SKIP 0(20c는 23c로 옮겨 뺌; 이 실행 때는 SKIP 1로 보고됨). `fault` 그룹 23a~23h 모두 PASS(R, int-level, int-tmplv, istack 기대값 일치). `runtests.sh` SAME 12, DIFF 1(knownbug, 예상). 11-24 ~ 11-25에 고친 원인은 `document/errors.md` 42~53번.

- 아직 시험하지 않은 것: 같은 사이클에 겹친 fault의 우선순위, syscall/iret 중 data fault(회로 미처리).

### 결과 (2026-11-23, `cpu-32.dig` 22:17 저장본, kernel 브랜치) 11-25에 해결

page fault(id 3)와 privilege fault(id 4)를 넣은 회로와 새 테스트 `int_fault`(23a~23h). 자세한 원인은 `document/errors.md` 39~45번.

- 21:13 저장본: 기존 PASS 44, SKIP 1 그대로. `runtests.sh` SAME 12, DIFF 1(knownbug).
- 22:17 저장본: `runtests.sh` SAME 13(selftest seed 하나 추가), DIFF 1(knownbug). inttest PASS 33, FAIL 15, SKIP 4.
  - 23a, 23b, 23g PASS. fault R = 명령 자신(errors.md 42).
  - 같은 수정으로 syscall R도 syscall 자신이 되어 무한 재진입: 8a, 8b, 14a~14c, 15a, 19a~19c, 20a, 20b, 21b, 22 FAIL/SKIP(42).
  - 23c, 23e: privilege fault 명령이 실행됨(43). 23d: fetch fault entry가 멈춤(44). 23f: 오경로 `stexp`가 남긴 가짜 privilege fault(45). 23h: 23d에서 멈춤.

### 결과 (2026-11-22, `cpu-32.dig` 23:17 저장본, kernel 브랜치)

PASS 44, SKIP 1(20c), 종료 코드 0. `runtests.sh`는 SAME 12, DIFF 1(knownbug, 예상).

- CPU가 kernel, mask 1로 시작해 `INIT`이 `int-level` = 0x80을 쓰고, 핸들러의 `int_pri`에 ker 비트(`PKER`)를 넣었습니다. 핸들러 level 기대값은 0x91(13, 14b, 15d), `iret` 뒤는 0x80(7c)입니다.
- 이날 찾아 고친 회로 문제: `index-compressor`의 SI4..7 위치(syscall의 ROB-INT 쓰기가 사라짐), ROB-INT data select(syscall이 id 0으로 진입), flush와 같은 사이클에 issue된 오경로 `ldexp`/`iret`이 serialize 래치(`RS_FF@5160,-2600`, `SRL_HALT`)를 남겨 frontend가 멈춤(14a, 14c, 15a, 16a, 19a). 마지막 것의 최소 재현은 `jmp.one main; nop; ldexp; iret; main:`입니다.
- 20b(errors.md #27)는 PASS가 되어 `XFAIL`에서 뺐습니다. 20c는 권한 fault가 id를 저장하기 전까지 실행하지 않습니다.

### 결과 (2026-11-19, `cpu-32.dig` 23:00 저장본, `IPU-test.dig` 13:35, `computer-32-test.dig` 14:20)

PASS 43, XFAIL 2, 종료 코드 0.

- 11-18의 XFAIL(14a, 14b, 14d, 15b, 15c, 16b, 19a~c, 21b, 22)은 모두 PASS가 되어 `XFAIL`에서 뺐습니다. 고친 회로 버그는 `errors.md` 25~38번에 있습니다.
- 16b: 높은 IRQ가 낮은 핸들러의 mask 0 구간 안에서 중첩된 경우가 175회, 낮은 쪽 `iret` 뒤에 들어간 경우가 66회입니다.
- 17b: 둘째 펄스 3개가 `I-act|I-acp` 중에 들어왔습니다.
- 21b: 출력 `APBEG000000B8 0000009F g`(R = `syscall`+4, `int-level` 0x9f).
- 11번(SKIP)은 Fl_INT를 없앨 예정이라 항목을 지웠습니다.

| 결과 | 항목 | 내용 |
|---|---|---|
| XFAIL | 20b | 진입이 `ker`를 켜지 않습니다. CPU가 ker를 `INT_lev` bit 4에서 가져옵니다. kernel 브랜치에서 다른 exception과 같이 구현합니다 |
| XFAIL | 20c | 권한 검사가 배선되지 않았습니다. user의 `stexp istack`이 실행되어 다음 `syscall`이 0x1230에 push합니다 |

### 결과 (2026-11-18, `cpu-32.dig` 01:14 저장본, 14번 이후 추가 뒤)

PASS 32, XFAIL 11, SKIP 3, 종료 코드 0. 1a~13은 그대로 PASS. 새 항목:

| 결과 | 항목 | 내용 |
|---|---|---|
| PASS | 10d~10f, 14c, 15a, 15d, 16a, 17a, 17b, 18, 20a, 21a | `iret` 주변 둘째 IRQ(183회), 오경로 `syscall`(849번 fetch, `INT_req` 0), 늦은 분기 주변 진입(168회) |
| XFAIL | 14a | `errors.md` 25번. `syscall` 복귀 PC가 다음 명령 + 4(케이스 5, 10 = 오분기 루프 끝 직후). `syscall`의 clr이 trap-wait 뒤에 오면 FT는 이미 멈춘 상태에서 hold(`D_FF@-200,1220`, D = clr & !wait)가 IAR을 한 번 더 진행시키고, decode가 받지 못한 그 명령을 재시작이 덮어씀 |
| XFAIL | 14b, 20b | `errors.md` 27번. `syscall` 핸들러의 `int-level`이 0x10(level 0). 진입이 `ker`를 켜지 않음 |
| XFAIL | 14d | `errors.md` 26번. 끝에서 `istack` = 0x3004(24개 NPAD 모두). `istack` ±4와 `stack`이 `addsub-4c32@2700,-180`을 같이 쓰고, `iret`의 +4가 Fl_JPU clr과 겹치면 `Mux@2620,-200`이 `CT_SC-_FD`를 받음 |
| XFAIL | 15b | `errors.md` 29번. `syscall`의 `INT_req` 2~14 half 전의 IRQ 펄스가 사라짐(33회, seed 3개 모두, `S`만 실행) |
| XFAIL | 15c | `errors.md` 25번. IRQ가 `syscall` 자리(R = sc)에서 들어간 뒤 다시 실행된 `syscall`의 복귀 PC가 + 4(H=17936, 3회). 14a와 같은 모양 |
| XFAIL | 16b | 11-18 (`cpu-32.dig` 13:07 저장본) 재실행: `errors.md` 28번(진입 때 `int-tmplv` 미기록)은 더 이상 안 나타남. 남은 18개는 mask를 다시 켜는 곳 주변: `close`(`stexp` 0x91, 0x1d8) 몇 반주기 전의 high 펄스가 그 뒤 1-3 명령에서 진입(R = 0x1dc..0x1e4). 회로 문제인지 테스트 창 문제인지 미정 |
| XFAIL | 19a(19b, 19c SKIP) | `errors.md` 30번. 둘째 `syscall`의 `iret`이 첫 복귀 PC(0x320)로 갔다가 stall |
| XFAIL | 20c | 권한 검사 미배선. user의 `stexp istack`이 실행되어 다음 `syscall`이 0x1230에 push |
| XFAIL | 21b | `int_pri[1]` = 0인 `syscall`에서 stall(규칙 미정) |
| XFAIL | 22 | `errors.md` 31번. `ld ; ld ; iret`(C5, C6)에서 `iret`이 돌아오지 않음: `TRAP_IRET`의 driver(`Driver@340,4080..4560`)가 dispatch 제어 버스를 모는 같은 사이클에 다음 명령이 dispatch되어 decoder driver(`Driver@1540,-1820` / `-1900`)도 `CT_FU-_WR` / `CT_FU-_INST`를 몰고, 그 뒤 클럭만 바뀜. C4는 그 사이클에 halt-frontend = 1이라 통과. `int_nest`의 level 2 핸들러는 그래서 `iret` 앞에 dummy `st`를 둠 |
| SKIP | 11 | 206회 모두 Fl_INT가 0. 진입은 Fl_JPU로 flush(PC가 핸들러가 되기 2 half 전)하므로 이 항목은 지금 모양으로는 날 수 없음 |

- 14c는 `int_sysprev`의 이전 배치(핸들러가 `istack`을 로그하기 전, NPAD 0~7)에서 NPAD 0~3이 첫 `iret`에서 stall했지만, 지금 배치로는 NPAD 0~23 모두 통과합니다.
- `int_sysirq`, `int_nest`는 `syscall`/IRQ 수락 RS_FF 경쟁(`errors.md` 24번) 때문에 zseed 1, 2, 3으로 돌립니다. 결과는 seed마다 같았습니다.

### 결과 (2026-11-18, `cpu-32.dig` 01:14 저장본, 14번 이전)

PASS 20(1a~10c, 13), SKIP 1(11: 38 위상 중 Fl_INT와 Fl_JPU가 겹친 실행 없음). `XFAIL`은 비어 있습니다. `int_sys`는 따로 zseed 1~24 × tap 2종 48회 모두 같은 출력이었습니다(수락 RS_FF의 S=R=1 경쟁, `document/errors.md` 24번, 이 고쳐진 뒤). 7a~7c, 8a, 8b가 통과하기까지의 회로 버그는 `errors.md` 19~24번입니다.

### 이전 결과 (2026-11-15, `inttest.py`, `computer-32-test.dig` 00:12 저장본, `prec`는 zseed 1, 2, 3, 나머지는 1)

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
