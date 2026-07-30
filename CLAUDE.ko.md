# CLAUDE.md (한국어)

이 파일은 Claude Code(claude.ai/code)가 이 저장소에서 작업할 때 참고하는 안내 문서입니다.

## 프로젝트 개요

**ISA-32** 명령어 집합 아키텍처 기반의 커스텀 32비트 비순서(out-of-order) CPU로, **hneemann Digital** 회로 시뮬레이터 위에서 동작합니다. 레지스터 리네이밍, 리오더 버퍼(ROB), 분기 예측(BTB), 리턴 어드레스 스택(RAS), 로드/스토어 버퍼를 구현합니다.

## 도구

### 어셈블러

ISA-32 어셈블리(`.txt`)를 바이너리로 컴파일합니다. 컴파일된 실행 파일은 `tools/bin/dasm.exe`에 있습니다.

```
tools/bin/dasm.exe <입력.txt> <출력.bin> [플래그]
```

디버그 플래그 (복수 사용 가능):
- `-file` — 원본 소스 출력
- `-token` — 렉서 토큰 출력
- `-preproc` — 전처리기 결과 출력
- `-eval` — 파싱/평가 결과 출력
- `-bin` — 바이너리 인코딩 출력
- `-double` — 이중 출력 모드

### 헥스덤프

```
tools/bin/hexdump.exe <파일.bin> [-width N] [-bundle N]
```

기본값은 한 줄에 4바이트. 어셈블된 바이너리 확인에 유용합니다.

### 도구 재빌드

소스는 `tools/assembler src/`(C++)와 `tools/hexdump src/`에 있습니다. C++17 이상 컴파일러로 빌드하며, 어셈블러 진입점은 `tools/assembler src/main.cpp`입니다.

## ISA-32 어셈블리 문법

### 레지스터

형식: `<reg_id>[<인덱스>].<모드>`

| ID | 설명 |
|----|------|
| `gen[N]` | 범용 레지스터 (N = 0..24) |
| `reg[N]` | gen의 별칭 |
| `zero`, `one`, `full` | 상수 레지스터 (0, 1, 0xFFFFFFFF) |
| `pc`, `stack`, `flag`, `sbp` | 특수 레지스터 |

모드: `32B` (전체 32비트), `16L` (하위 16비트), `8L`, `8H`, `S16H` (상위 16비트 세팅), `S16L`, `S8L`, `S8H`

### 메모리 접근

```
ld.<기준레지스터> <목적레지스터>, <오프셋16>    ; mem[base + offset] 로드
st.<기준레지스터> <소스레지스터>, <오프셋16>    ; mem[base + offset] 저장
```

### 제어 흐름

```
jmp.<플래그> <레지스터>, <imm16>    ; 조건부 상대 점프
ijmp.<플래그> <레지스터>, <imm16>   ; 간접 점프 (조건 반전)
call <레지스터>, <imm16>            ; 함수 호출
ret                                 ; 반환
```

플래그 조건: `zero`, `neg`, `pos`, `carry`, `carry4`, `overflow`, `one` (항상), `gen`

### 전처리기

```asm
#define NAME (expr)           ; 상수 치환
#macro NAME 인수개수          ; 매크로 정의 (인수는 #1, #2, ... 로 참조)
    ...본문...
#end
##로컬이름                    ; 매크로 展開마다 고유한 레이블 생성 (이름 충돌 방지)
```

### 섹션

```asm
.text        ; 명령어
.data        ; 초기화된 데이터: <크기식>, <값식>
.bss         ; 미초기화 데이터: <크기식>
레이블:      ; 레이블 정의 (주소 상수로 사용 가능)
```

### 명령어 인코딩 (32비트, big-endian)

`[opcode 8비트][reg_a 8비트][reg_b 8비트][imm16 또는 reg_c 8비트]`

opcode 표는 `tools/assembler src/assembler.hpp`의 `instructionBase` 맵에 있습니다.

## CPU 마이크로아키텍처 (`components/` 내 Digital 회로 파일)

- **`computer-32.dig`** — 최상위 시스템 (CPU + 메모리 + IO)
- **`cpu-32.dig`** — 메인 CPU: 페치 → 디코드 → 디스패치 → 실행 → 커밋 파이프라인
- **`res-station.dig`** — 예약 스테이션 (명령어 윈도우 크기 16)
- **`ROB-gen.dig`** — 순서대로 커밋하기 위한 리오더 버퍼
- **`regFile*.dig`** — 리네이밍이 적용된 투기적 레지스터 파일
- **`BTB.dig`** / **`BTB-buff.dig`** — 분기 타겟 버퍼 (크기 16, 2비트 예측기, 12비트 태그, 4비트 인덱스)
- **`RAS.dig`** — 리턴 어드레스 스택 (투기적 4모듈 + 아키텍처 8모듈)
- **`load-buffer.dig`** / **`store-buffer.dig`** — 메모리 연산 버퍼
- **`id-manager.dig`** — 명령어 ID 할당 및 추적
- **`alu-32.dig`** — 32비트 ALU

### 현재 개발 중 (dev 브랜치)

로드/스토어 버퍼의 순서 관리를 큐 방식에서 **age matrix** 방식으로 교체 중입니다. Age matrix는 명령어 간 상대적 나이를 인코딩하며, 비트가 1이면 현재 엔트리가 해당 인덱스 엔트리보다 오래된(먼저 발행된) 것을 의미합니다.

## 프로그램

`program/` 디렉터리의 샘플 프로그램은 모두 ISA-32 어셈블리로 작성되어 있습니다. `add_terminal.txt`는 IO 입출력, 함수 호출, 스택 사용, BCD-십진수 변환을 보여주는 완성된 예제입니다.

IO 매핑 메모리 베이스: `0x4000000` (KEY_IN, KEY_V, KEY_R, TERM_OUT 오프셋 0/4/8/12)
