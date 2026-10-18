# calc — 터미널 부동소수점 계산기

`program/calc/calc.asm`. 수식을 한 줄 입력하면 float로 계산해서 출력합니다. `floatingpoint`와 `io` 라이브러리를 씁니다.

```
float calc (+ - * / (), q = quit)
> 1.5 * (2 + -0.5) / 4
= 0.5625
> 1 + * 2
      ^ error
```

## 빌드

```
; program/ 에서
dasm calc/calc.asm -o calc.o
dasm floatingpoint/float.asm -o float.o
dasm io/io.asm -o io.o
dlnk calc.o float.o io.o -o calc/calc.bin        ; 회로에 올릴 때는 -o ../boot/rom.bin
```

`calc.asm`은 `calc/` 안에 있으므로 include 경로가 `#include<../io/io.hsm>`, `#include<../floatingpoint/float.hsm>`입니다.

## 실행

- 에뮬레이터: `python ../tools/emu/isa32emu.py calc/calc.bin "1+2*3\nq\n"`
- Digital 회로: `boot/rom.bin`으로 링크하고 `components/cpu-32 test.dig`를 실행합니다(루트 `CLAUDE.md`의 "Running on the circuit").
- 헤드리스 회로 시뮬레이션(dsim, `tools/dsim/`에서):
  ```
  java -Dlines=1 -Dbanner=zzzz -cp "$J;build" Sim "../../components/cpu-32 test.dig" - "1+2|q" 3000000
  ```

## 사용법

- 수식을 한 줄로 입력합니다. `*` `/`가 `+` `-`보다 먼저 계산되고, 같은 우선순위는 왼쪽부터 계산합니다.
- 괄호, 단항 `-`/`+`, 토큰 사이 공백을 지원합니다. 숫자는 `fp_str2f` 문법이라 `1e-3`, `.5`, `inf`, `nan`도 됩니다.
- 결과는 유효숫자 7자리(`DIGITS`)로 출력합니다.
- 문법 오류가 나면 처음 잘못된 위치 아래에 `^ error`를 표시합니다.
- `q`는 종료(`halt`), 빈 줄은 무시합니다. 백스페이스는 `io_getline`의 다시 출력 방식으로 동작합니다.

문법:
```
expr   = term   { ('+' | '-') term }
term   = factor { ('*' | '/') factor }
factor = '-' factor | '+' factor | '(' expr ')' | number
```

## 구조

- 재귀 하강 파서(`parse_expr` → `parse_term` → `parse_factor`).
- 커서는 `gen[12]`, 첫 오류 위치는 `gen[11]`(0이면 오류 없음). 둘 다 라이브러리가 보존하는 레지스터라 호출 사이에 유지됩니다.
- 숫자는 `fp_str2f`의 끝 포인터로 다음 위치를 찾습니다.
- 입력 버퍼는 `LINE`(128), 프롬프트 폭은 `PROMPT_W`(2, 오류 `^` 위치 계산용).

## 상태

- 에뮬레이터: 우선순위, 괄호, 단항 `-`, `inf`/`nan`/`-0`, 오류 위치, 백스페이스 다시 출력을 확인했습니다.
- Digital 회로 (dsim, 2026-10-17): 지금 소스 그대로는 **아무것도 출력하지 않고 멈춥니다.** `quit:`의 `halt`가 바로 앞 `jmp.one zero.32B, loop` 다음에 있어서, 회로가 이 `halt`를 투기적으로 실행해 버립니다(루트 `CLAUDE.md`의 알려진 회로 문제). `halt`를 `jmp.one pc.32B, 0`으로 바꾸면 `> 1+2` → `= 3`이 정상으로 나옵니다.
- 수식 한 줄에 수만~수십만 명령이 듭니다. 대부분 `fp_str2f`/`fp_f2str` 비용이라 Digital에서는 느립니다.
