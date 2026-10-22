# TODO — assembler-linker (2026-10-17 기준)

## 현재 상태 (2026-10-17)

| 작업 | 커밋 | 배포 |
|---|---|---|
| `-big`/`-little`, 플래그 64개 (u64) | `0935e4a` | O |
| dlnk `-org N` | `33aa383` | O |
| 섹션 이름 mangling + `global section [name]`, 이름 있는 섹션 버그 수정 | `33aa383` | O |
| ISA 시트에 맞춘 인코딩 (pctl/getabs/cache/atom/intervec) | `33aa383` | O |
| dlnk 출력 헤더 {start, length}, `-raw`, `-start <label>` | 2026-10-17 커밋 | O |
| `.text`에서 `equ` 허용 (table version 414) | 2026-10-17 커밋 | O |

- 배포 위치: `computer 32 (new)/bin`, `digital computer 32/tools/bin` (+ tools 소스 미러).
- 회귀: `test/program` 25/29 성공, `-raw` 출력이 배포본과 바이트 단위로 동일.
- `digital computer 32` 쪽 (computer32 manager 세션, 2026-10-17, 미커밋):
  - 진입 프로그램(calc, pi, fp_harness, io_harness, memtest*, selftest)에 `global _start` / `.text` / `_start:` 추가.
  - `fp_test.py` / `io_test.py` / `pi_test.py`가 dlnk에 `-raw`를 붙임. ALL PASS.
  - 엔디안: Digital/dsim 시뮬레이터는 리틀엔디안(기본), Python `isa32emu.py`는 빅엔디안이라 세 스크립트는 `-big` 유지.

## 완료됨 (2026-10-12 ~ 10-17)

### 어셈블러 (dasm)
- `-big` / `-little` (기본 little). 오브젝트 헤더에 `endian` 필드 추가 → 예전 `.o`는 다시 어셈블해야 함.
- 섹션 이름을 `파일명#이름`으로 mangle. 파일 사이에서 같은 섹션을 합치려면 `global section 이름` (이름 생략 = 이름 없는 섹션).
- 이름 있는 섹션(`.text a`)의 이름이 항상 `""`로 기록되던 버그 수정 (`section_unit` 문법).
- ISA 시트(xlsx)에 맞춤: `pctl` 7C + byte 3 en/dis, `getabs` 7D, `flush`/`inval` = `cache` 7E + md 비트, `ALL` 0x40, `atom` 7F 추가, `intervec R, m, d`.
- `.text`에서 `equ` 사용 가능.
- `org` 문법은 구현했다가 롤백함 (링커 `-org`로 대체).

### 링커 (dlnk)
- `-big` / `-little`, 오브젝트와 바이트 순서가 다르면 오류.
- `-org N`: 첫 그룹의 시작 주소.
- 출력 헤더 (기본): `{시작 라벨 주소, 프로그램 길이}` 각 4바이트. `-start <label>` (기본 `_start`), `-raw`면 헤더 없음.
  - ⚠️ 기본이 헤더 포함이라 `_start`가 없는 프로그램은 `-raw`나 `-start`가 필요함 (`test/program` 전부 해당).

### hexdump
- `-big` / `-little`: little이면 묶음 안의 바이트를 뒤집어 표시.

## 완료됨 (커밋 `5cd5a7e` 이후 작업, 2026-10-11 커밋에 포함)

## 완료됨 (커밋 `5cd5a7e` 이후 작업, 2026-10-11 커밋에 포함)

### 어셈블러 (dasm)
- `.bss` 크기를 오브젝트에 기록 (`Section::size`). 여러 파일의 bss가 같은 주소에 겹치던 버그가 해결됨.
  - ⚠️ 오브젝트 포맷이 바뀌었으므로 예전 `.o`는 다시 어셈블해야 함.
- `global` / `extern` 파싱과 의미 구현.
  - global 이름은 접두사 없이 내보내고, 나머지 이름은 `파일명#이름`으로 mangle함.
  - extern 이름은 항상 재배치 대상.
  - 오류 추가: `GLOBAL_UNDEFINED`, `EXTERN_DEFINED`.
- BUGS.md의 3건 수정.
  - 인자 1개짜리 매크로 동작 수정, 인자 개수 오류와 괄호 생략 오류 추가.
  - `;`만 있는 줄도 주석으로 처리 (테이블 version 407).
  - 실패하면 출력 파일을 만들지 않음.
- 그 전에 한 작업:
  - eval 오류에 줄/토큰/오류 이름 표시, `srcToken`을 값으로 복사.
  - `DataValue::length` 수정.
  - `equ`와 `$` 평가 연결.
  - `#ifdef` / `#ifndef` 추가, 전처리 오류 보고.
  - UTF-8 주석에서 생기던 UB 수정.

### 링커 (dlnk)
- `Format::loadRaw`가 오브젝트를 검증함. 깨진 오브젝트는 segfault 대신 `invalid object file` 오류.
- `undefined symbol` / `duplicate symbol` 오류 보고.
- bss를 크기 기준으로 배치.

### 문서 / 배포
- 문서:
  - `SYNTAX.md` 작성.
  - `digital computer 32/ISA.md` 작성.
  - `CLAUDE.md` 56~58번 항목 추가.
- 배포:
  - `computer 32 (new)/bin`과 `digital computer 32/tools`의 dasm/dlnk 교체, table.bin 재생성.
  - tools에 소스, SYNTAX.md, BUGS.md 반영.
- 회귀 테스트:
  - `test/program` 29개 중 25개 성공, 배포본과 바이트 단위로 동일.
  - BUGS 재현 3건 해결 확인.
  - float 예제 어셈블+링크 성공.

## 남은 일

### 커밋
- [x] 이 저장소: 2026-10-11 커밋 (저장소 루트가 `src/`로 바뀐 뒤 첫 커밋).
- [x] 이 저장소: 2026-10-12 `0935e4a`/`9e8f624`, 2026-10-17 `33aa383`.
- [x] 이 저장소: dlnk 헤더(`-raw`/`-start`) + `.text` equ 커밋, origin/dev push (2026-10-17).
- [ ] `digital computer 32` 저장소: `_start` 추가와 테스트 스크립트 `-raw` 수정 (computer32 manager 세션 작업) 커밋.
- [ ] `digital computer 32` 저장소: ISA.md와 tools가 커밋되지 않음 (2026-10-11 tools 재배포분 포함). 다른 세션(floatingpoint)의 변경도 섞여 있으니 범위를 나눠서 커밋할 것.
- [ ] 추후 이 저장소(`src/`)를 `digital computer 32` 저장소로 git 통합.
  - 지금은 소스를 `digital computer 32/tools` 미러에 수동으로 복사하고 있음. 통합하면 복사가 필요 없음.
  - 이 저장소의 커밋 히스토리를 보존하는 방식(subtree 등)으로 합칠 것.

### 목표
- [x] linker: bss padding 구현. (`-padbss` 플래그, `linker::bssPadding`)
- [x] assembler: 문자열 추가
- [x] assembler: 리스트 추가
- [x] assembler / linker: `-o <출력 파일>` 플래그 추가 (현재는 위치 인자로 출력 파일을 받음)
- [x] 터미널 입출력 통일 (`CLI.md`): `iotools::Terminal`/`errPosGenerator`, dasm·dlnk·hexdump 적용, GCC 형식 위치 오류, 종료 코드 0/1/2.
- [ ] `isa32emu.py`(Python 에뮬레이터)를 리틀엔디안으로 바꿀지 결정. 바꾸면 fp/io/pi_test.py의 `-big`을 뺌.
- [ ] 시뮬레이터 로더가 dlnk 헤더를 읽게 할지 결정 (지금은 스크립트가 `-raw` 사용).
- [ ] `-align [byte]` 플래그로 word 정렬 (값 생략 시 4바이트).
  - linker: 섹션 조각과 text/data/bss 그룹의 시작 주소를 맞추고, 그 사이는 0으로 채움.
  - assembler: 크기가 4의 배수가 아닌 allocate 뒤의 데이터/라벨을 맞춤 (아래 "데이터 배치 한계" 참고).
  - 값은 2의 거듭제곱만 허용. 값을 생략할 수 있는 플래그라 인자 파싱 규칙을 정해야 함 (터미널 입출력 통일안과 같이).

### 알려진 미해결 문제
- [x] 파서(LALR) 실패: branch, factorial_norec, prime_old. 사용자 판단으로 보류 중.
- [x] 소스 쪽 오류 (어셈블러 문제 아님):
  - bcdtodec: 소스에 없는 라벨 참조.
  - texteditor: `#define LSET 2`는 `#macro`로 써야 함.
- [ ] 데이터 배치 한계:
  - 4바이트보다 큰 allocate에 재배치 값이 들어가면 첫 워드만 패치됨.
  - 크기가 4의 배수가 아니면 뒤 데이터가 정렬되지 않음.
- [ ] 중첩 `#include`의 경로 기준이 항상 최초 입력 파일의 디렉터리 (`PreprocData::fileName`이 갱신되지 않음).
- [ ] `gen[n]` 인덱스 범위 검사 없음 (25 이상이면 특수 레지스터 id와 겹침).
- [ ] global/extern 오류 메시지의 토큰이 이름이 아니라 키워드로 표시됨.
- [x] ISA 시트(xlsx)와 다른 부분: `pctl`/`getabs`/`cache`(flush·inval)/`atom`/`intervec` 시트에 맞춤 (2026-10-17).
- [ ] ISA 남은 확인 (ISA.md 6절): md 비트 방향(flush=1 가정), `rol`/`ror` 방향, `jp`/`jpi`/`break` 이름.
- [x] 문자열 리터럴(`"..."`): `.data`의 `total, "str"`로 연결됨 (2026-10-11).
- [ ] `digital computer 32` 쪽 CLAUDE.md의 도구 설명이 예전 상태.
