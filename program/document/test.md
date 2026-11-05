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

- `flushstall` **DIFF**: `c10`, `c30`, `c31`, `c32`, `c33`이 `FAIL 00050100`입니다(zseed 1/2/3 모두 같음). call 뒤 poison `addi`가 한 번 더 실행됩니다(4 대신 5). 나머지 34개 케이스는 PASS.
- `ldstmix` SAME (모두 PASS). store 응답을 다시 켜면 확인할 것.
- `knownbug` DIFF: k1..k3 FAIL(`000000AA`, `0000BEEF`, `000000CC`: word 전체를 씀). `h 123`은 맞게 나옵니다. 분기 뒤 `halt` 문제는 이 경우에는 재현되지 않습니다.

## fill.asm 상태 (2026-10-17, dsim)

- 에뮬레이터에서는 14400픽셀을 다 칠하고 끝납니다.
- 회로에서는 **픽셀 1개(주소 0)만 칠하고 멈춥니다.** 루프 끝 `jmp.neg` 바로 뒤의 `halt`를 회로가 분기가 결정되기 전에 실행해 버리기 때문입니다(루트 `CLAUDE.md`의 알려진 회로 문제).
- `halt`를 `jmp.one pc.32B, 0`으로 바꾸면 14400픽셀(주소 0~0x383F)이 모두 칠해지는 것을 확인했습니다.
- `GR_SIZE`는 `90*160*4`바이트입니다. `boot/src/boot.asm` 주석의 "display 160 x 80"과 다르지만, 회로의 GraphicCard 설정은 높이 90입니다.
