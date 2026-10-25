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

`M4.o`, `M7.o`, `fill.o`는 예전 빌드 결과입니다.

## fill.asm 상태 (2026-10-17, dsim)

- 에뮬레이터에서는 14400픽셀을 다 칠하고 끝납니다.
- 회로에서는 **픽셀 1개(주소 0)만 칠하고 멈춥니다.** 루프 끝 `jmp.neg` 바로 뒤의 `halt`를 회로가 분기가 결정되기 전에 실행해 버리기 때문입니다(루트 `CLAUDE.md`의 알려진 회로 문제).
- `halt`를 `jmp.one pc.32B, 0`으로 바꾸면 14400픽셀(주소 0~0x383F)이 모두 칠해지는 것을 확인했습니다.
- `GR_SIZE`는 `90*160*4`바이트입니다. `boot/src/boot.asm` 주석의 "display 160 x 80"과 다르지만, 회로의 GraphicCard 설정은 높이 90입니다.
