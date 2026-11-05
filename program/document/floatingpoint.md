# floatingpoint — 소프트웨어 부동소수점 라이브러리

ISA-32 어셈블리로 작성한 **IEEE-754 binary32(float)** 라이브러리입니다. 이 CPU에는 FPU가 없어서 정수 명령으로 구현했습니다.
공통 규약(헤더/구현 분리, ABI, 문자열, 주소 제약, 의존하는 명령 의미)은 루트 `CLAUDE.md`의 "Programs" 절을 따릅니다.

## 파일 (`program/floatingpoint/`)

| 파일 | 역할 |
|---|---|
| `float.hsm` | 정의(API). 함수 `extern` 선언, 공개 상수(`FP_STRBUF`), 매크로(`FP_LDB`, `FP_SET`, `FP_PUTC`). 사용하는 프로그램에서 include합니다. `#ifndef FP_H` 가드 |
| `float.asm` | 구현. `#define FP_IMPL 1` 후 `float.hsm`를 include하고(그래서 `extern`이 빠짐), 함수를 `global`로 내보냅니다 |
| `fp_harness.asm` | 테스트용 프로그램. mailbox에 연산과 인자를 넣고 실행한 뒤 `halt` |
| `fp_test.py` | harness를 빌드해서 에뮬레이터로 돌리고 기준값과 비교합니다 |

## 빌드 / 사용법

```asm
#include<floatingpoint/float.hsm>     ; 경로는 dasm에 넘긴 파일의 디렉터리 기준 (calc/에서는 ../floatingpoint/...)
```

```
; program/ 에서
dasm main.asm -o main.o
dasm floatingpoint/float.asm -o float.o
dlnk main.o float.o -o main.bin     ; main.o를 먼저
```

- 오브젝트에서는 내부 라벨이 `float#...`로 저장됩니다. `global`로 내보낸 6개 함수 말고는 다른 파일과 이름이 충돌하지 않습니다. 라벨은 모두 `fp_` 접두사를 씁니다.
- `float.o`가 먼저 링크되더라도, 첫 명령 `jmp.one zero.32B, fp_lib_end`가 라이브러리를 건너뜁니다.
- `.data`에 `fp_pow10`, `fp_bigA`, `fp_bigB`, `fp_digits`(약 160 word)를 차지합니다.
- **주소 제약**: 코드와 데이터가 모두 0x8000 미만이어야 합니다. `call zero.32B, fp_xxx`, `jmp.* zero.32B, label`, `ld.gen[13] …, fp_pow10` 등이 라벨을 부호 있는 16비트 offset으로 쓰기 때문입니다. 링크하면 모든 text 뒤에 data가 오므로 프로그램 text가 커지면 `fp_pow10`도 뒤로 밀립니다(`fp_harness` 링크 결과에서 0x186C).
- **재진입 불가**: `fp_str2f`와 `fp_f2str`는 전역 버퍼 `fp_bigA`, `fp_bigB`, `fp_digits`를 씁니다. 다른 `fp_` 호출을 중단시킬 수 있는 인터럽트 핸들러에서는 부르면 안 됩니다. `fp_add/sub/mul/div`는 레지스터와 스택만 쓰지만, ISR에서 부르려면 `gen[1..4]`, `gen[13..20]`을 ISR이 직접 저장해야 합니다.
- 스택은 깊이 약 12 word를 씁니다.

## API

| 함수 | 입력 | 출력 |
|---|---|---|
| `fp_add` | `gen[1]` a, `gen[2]` b | `gen[0]` = a + b |
| `fp_sub` | 같음 | `gen[0]` = a - b (b의 부호를 뒤집고 `fp_add`로 넘어감) |
| `fp_mul` | 같음 | `gen[0]` = a * b |
| `fp_div` | 같음 | `gen[0]` = a / b |
| `fp_str2f` | `gen[1]` = 문자열 주소 | `gen[0]` = float, `gen[1]` = 끝 포인터(사용하지 않은 첫 글자, strtod와 같음) |
| `fp_f2str` | `gen[1]` = float, `gen[2]` = 버퍼, `gen[3]` = 유효숫자(1..9, 범위 밖은 클램프) | 버퍼에 문자열, `gen[0]` = 길이 |

- **문자열**: 1 바이트 = 1 글자, 0으로 끝남(2026-10-17에 word 문자열에서 바꿈). 글자는 `FP_LDB`로 읽고, `fp_f2str`는 `st .8L`로 씁니다.
- **`fp_str2f` 입력 문법**: `[공백][+|-](숫자[.숫자] | .숫자)[(e|E)[+|-]숫자]`, 그리고 `inf`/`nan`(대소문자 무관, 세 글자 전체 확인). 파싱할 수 없는 문자가 나오면 거기서 멈추고 그때까지 읽은 값을 돌려줍니다.
  - 숫자가 하나도 없으면(`""`, `"-"`, `"."`, `"abc"`, `"inx"`) `+0`을 돌려주고 끝 포인터는 시작 주소 그대로입니다. 호출자는 이것으로 파싱 실패를 압니다.
  - `"1e"`, `"1e+"`처럼 지수 뒤에 숫자가 없으면 끝 포인터는 `e`를 가리킵니다. `"infinity"`는 `inf`까지만 읽습니다.
  - 알려진 차이: 앞 공백은 스페이스만 건너뜁니다(탭은 아님). 지수 값은 1000을 넘으면 더 쌓지 않아서, 가수가 1000자리를 넘는 극단적인 입력은 틀릴 수 있습니다.
- **`fp_f2str` 출력**: C의 `printf("%.*g")`와 같습니다. `-4 <= X < P`이면 고정소수점, 아니면 `d.ddde±XX`, 끝의 0은 지웁니다. 특수값은 `inf`, `-inf`, `nan`, `-0`(음수 NaN도 `nan`, glibc는 `-nan`). 버퍼는 최대 17바이트를 쓰므로 `FP_STRBUF`(20)바이트를 잡습니다. 유효숫자 9이면 왕복 변환이 정확합니다.

### IEEE 준수 범위

- 최근접 짝수 반올림(RNE)만 지원합니다. 다른 반올림 모드와 예외 플래그는 없습니다.
- 비정규수 입출력, ±0 부호 규칙: `(+0)+(-0) = +0`, 정확히 상쇄되면 `+0`.
- 오버플로는 ±inf. NaN 입력은 전파합니다(a가 NaN이면 a, 아니면 b, quiet bit 세움).
- invalid 연산(`inf-inf`, `0*inf`, `0/0`, `inf/inf`) 결과는 `0x7FC00000`, `x/0`은 ±inf.
- 문자열 변환도 정확히 반올림합니다(큰 정수 연산). `fp_str2f`는 유효숫자 **12자리(`FP_MAXDIG`)까지 정확히** 읽고, 그 뒤 자리는 sticky로만 반영합니다(IEEE 754-2008 5.12.2, binary32는 H ≥ 12).

## 내부 구조

- **unpack** (`fp_unpack`): class(0 zero / 1 finite / 2 inf / 3 nan), sign, exp, mant. finite 값은 `mant * 2^(exp-150)`, `mant`는 [2^23, 2^24). 비정규수는 정규화하므로 exp가 0 이하일 수 있습니다.
- **pack** (`fp_pack`): 입력 sign(`gen[13]`), biased exp(`gen[14]`), q(`gen[15]`, < 2^31), 값은 `q * 2^(exp-157)`. 모든 연산이 이 함수를 거칩니다.
  1. q[30]이 1이 되게 정규화
  2. exp ≥ 255이면 inf
  3. exp ≤ 0이면 sticky를 유지하며 오른쪽 시프트(비정규수)
  4. 하위 7비트로 RNE 반올림
  5. `((exp-1)<<23) + q`로 합침(숨은 비트와 반올림 올림수가 지수로 넘어감)
- **add**: 절대값이 큰 쪽을 a로 놓고, 가수에 guard 6비트를 붙여 정렬(sticky 보존) 후 더하거나 뺍니다.
- **mul**: 24비트 가수를 12비트씩 나눠 부분곱 4개(`fp_umul`, shift-add)를 `H*2^24 + L`로 조립. 캐리 플래그를 쓰지 않습니다.
- **div**: 복원식 나눗셈으로 몫 31비트, 나머지가 0이 아니면 sticky.
- **bigint** (`fp_big_*`): 16비트 limb 14개(224비트, `FP_BIGN`), little endian. 캐리/borrow는 시프트와 마스크로 계산. `fp_bigdiv`는 A/B를 31비트 정규화 몫 q, 지수 E, sticky로 돌려줍니다.
- **str2f**: 문자열을 `N * 10^K`로 파싱하고 크기 범위를 먼저 판정(`nd+K ≥ 40`이면 inf, `≤ -46`이면 0). `10^K`를 분자나 분모에 곱한 뒤 `fp_bigdiv`와 `fp_pack`으로 반올림.
- **f2str**: 십진 지수를 `X ≈ floor((exp-127)*1233/4096)`로 추정하고 `D = round(v * 10^(P-1-X))`를 bigint로 정확히 계산. D가 [10^(P-1), 10^P) 밖이면 X를 ±1 하고 다시 계산합니다(E = -1도 허용해야 무한루프가 안 생김, 테스트에서 찾은 버그). 마지막에 `%g` 형식으로 출력.

## 테스트

```
cd program/floatingpoint
python fp_test.py --quick        # 수 분
python fp_test.py --seed 7       # 전체 (약 9,000건)
```

- `../../bin`의 dasm/dlnk로 `fp_harness.asm`와 `float.asm`를 임시 폴더에 빌드합니다.
- harness의 mailbox(매직 `0x7E57F00D` 다음 word들)에 연산과 인자를 넣고 실행하고, 매번 `gen[5..12]` 보존과 스택 균형을 확인합니다.
- 기준값: 사칙연산은 numpy float32와 비트 단위로 비교(NaN은 quiet bit만 확인), `str2f`는 `Fraction`으로 구한 정확한 반올림 값, `f2str`는 Python `'%.*g'`, 그리고 9자리 왕복.
- **현재 결과 (2026-10-17)**: 에뮬레이터에서 ALL PASS(add/sub/mul/div 각 2,076건, str2f 446건(값과 끝 포인터), f2str 424건, 왕복).
- Digital 회로: 라이브러리 단독 테스트는 아직 안 돌렸습니다. 이 라이브러리를 쓰는 calc는 dsim에서 `1+2` → `= 3`이 나왔습니다(2026-10-17, `calc.md` 참고).

### 성능

- 문자열 변환은 bigint 연산 때문에 한 번에 약 1.5만~10만 명령이 걸립니다(측정 최대 102,319). Digital 시뮬레이션에서는 느립니다.
- 사칙연산은 그보다 훨씬 가볍습니다.
- 줄이려면 bigint 연산을 실제로 쓰는 limb 수만큼만 돌리면 됩니다(현재는 항상 14 limb).

## 남은 일 / 아이디어

- Digital 회로에서 `fp_harness` 실행 검증
- 추가할 만한 함수: 비교(`fp_cmp`), int↔float 변환, `fp_neg`/`fp_abs`, sqrt
- bigint limb 길이를 동적으로 바꿔 문자열 변환 속도 개선
