# dsim — Digital 회로 headless 시뮬레이터

`Digital.jar`(hneemann Digital 0.31)를 라이브러리로 써서 CPU 회로(`components/computer-32-test.dig`, 예전 `cpu-test.dig`)를 GUI 없이 돌립니다. 키 입력을 넣고, 터미널 출력을 받고, probe/tunnel 값을 반클럭마다 CSV로 덤프합니다. floating 버스 값이 무작위라서 같은 입력이어도 실행마다 결과가 달라질 수 있습니다. `-Dzseed=N`을 주면 같은 N에서 결과가 재현됩니다.

## 빌드

```
J="D:/Users/taeh5/Digital/Digital 0.31/Digital.jar"
javac -cp "$J" -d build Sim.java NetQuery.java Strip.java NetSig.java Pins.java
```

## Sim : 실행

```
java [옵션] -cp "$J;build" Sim <circuit.dig> <program.bin> <keys> [maxHalfCycles]
```

- `keys`: 프로그램이 `> `를 출력할 때마다 한 글자씩 넣습니다. `-Dlines=1`이면 `|`로 나눈 한 줄씩 `\n`을 붙여 넣습니다(calc용). 키를 다 쓴 뒤 다음 `> `가 나오면 끝냅니다(`""`이면 첫 `> `에서 끝).
- 끝날 때 `[half=... keysFed=... PC=... stop: 이유]`를 출력합니다. 이유는 `keys used up`, `restart`, `stall N`, `idle N`이고, `maxHalfCycles`(기본 2000000)까지 돌았으면 비어 있습니다.
- 옵션 (`-D이름=값`, `Sim` 앞에 씀)

| 옵션 | 뜻 |
|---|---|
| `dip=x,y;f.dig@x,y;...` | 켤 DipSwitch 위치. `x,y`는 top, `f.dig@x,y`는 하위 회로 `f` 안. 기본 `-560,-1640`(cpu-test의 클럭 mux를 Clock 쪽으로), `dip=`이면 하나도 켜지 않습니다. 그 위치에 DipSwitch가 없으면 에러로 멈춥니다(옮겨진 스위치 때문에 클럭 없이 아무 일도 안 하는 실행을 막음) |
| `set=E=1,run=1` | top-level 입력. `set=`이면 없음(top에 In이 없는 회로) |
| `at=H:k=v,...;H:k=v` | 반클럭 H에 top-level 입력을 바꿉니다. 예: computer-32 시작 순서 `-Dset=E=0,run=0,boot=1 -Dat="10:E=1,boot=0,run=1"` (`document/cpu-32.md` §4) |
| `banner=text` | 리셋 때 한 번 출력되는 글자. 다시 보이면 재시작으로 판정합니다(기본 없음 = 재시작 검사 안 함) |
| `stopRestart=0` | 재시작해도 계속 실행 |
| `stall=N` | `PC`, `com1`, `com2`가 N 반클럭 동안 그대로면 멈춘 것(halt, deadlock)으로 보고 끝냅니다. 기본 2000, `0`이면 끔. 볼 signal은 `stallCols=a,b,...` |
| `idle=N` | N 반클럭 동안 터미널 출력이 없으면 끝냅니다(프로그램 끝의 `jmp $` 등). 기본 0 = 끔 |
| `tap=top;ROB-SC.dig;...` | 해당 회로의 tunnel마다 probe 추가. 이름은 `T_<넷>`(top) 또는 `<파일>/<넷>`. `f.dig:넷=이름,넷`이면 적은 넷에만 붙이고 `=이름`으로 이름을 바꿉니다(예: `cpu-32.dig:PCS=PC,com1,com2`). 없는 넷이면 에러 |
| `csv=파일` | 모든 signal(probe, In/Out)을 반클럭마다 덤프. `csvFrom=N`이나 `csvKey=N`(N번째 키부터) |
| `probe=f.dig@x,y=이름;...` | 이름 없는 넷 좌표에 probe 추가. `f`는 `top`이나 하위 회로 파일(예: `res-station-debug.dig`) |
| `lines=1` | 줄 단위 입력 |
| `zseed=N` / `zseed=zero` | floating(high-Z) 비트를 읽을 때 쓰는 난수를 고정합니다. Digital은 값마다 `new Random()`을 써서 읽을 때마다 다른 값을 주므로, 이 옵션이 없으면 실행마다 결과가 달라질 수 있습니다. `zero`면 항상 0 |
| `hash=파일` | 반클럭마다 모든 signal의 해시 한 줄. 두 실행이 처음 갈라지는 반클럭을 찾는 용도 |
| `delta=파일` | `csv`처럼 덤프하되 바뀐 값만 `half 열번호 값`으로 씁니다(첫 줄은 이름 목록) |
| `rom=파일` | `boot/rom.bin`을 읽는 ROM이 대신 이 파일을 읽습니다. `boot/rom.bin`을 건드리지 않으므로 다른 프로그램을 동시에 돌릴 수 있습니다. ROM이 있는 회로는 `romCircuit=`(기본 `rom-test.dig`) |

- `program.bin`에 `-`를 주면 ROM이 자기 파일(`boot/boot.bin`, `boot/rom.bin`)을 읽습니다. 지금 회로에는 program memory로 지정된 ROM이 없으므로 `-`를 씁니다.

예 (tools/dsim에서, 3번째 키부터 덤프):

```
java -Dtap=top -Dcsv=t3.csv -DcsvKey=3 -cp "$J;build" Sim "../../components/cpu-test.dig" - "123"
```

## runtests.sh : 회귀 테스트

```
bash tools/dsim/runtests.sh [-j N] [-nobuild] [-c 회로.dig] [이름...]
```

- 테스트 프로그램을 빌드해 `components/<회로>`에서 병렬로 돌리고(`-Drom`), 터미널 출력을 에뮬레이터와 비교해 `== 이름 SAME|DIFF`를 출력합니다. git-bash에서 실행하고, `build/`에 Sim이 컴파일되어 있어야 합니다.
- 회로는 기본 `computer-32-test.dig`(2026-11-08부터, `DSIM_CIRCUIT` 환경 변수나 `-c`로 바꿈)입니다. 회로별 Sim 옵션은 스크립트의 `COPT`입니다.
  - `computer-32-test.dig`: top의 DipSwitch `-780,580`(E), `-780,620`(run)을 켜고 `-Dset=`. `cpu-32`에는 probe가 없으므로 stall 검사용으로 `-Dtap=cpu-32.dig:PCS=PC,com1,com2`. 다른 신호는 `-Dtap=cpu-32.dig:넷,...`이나 `-Dprobe=cpu-32.dig@x,y=이름`으로 그때마다 붙입니다.
  - `cpu-test.dig`(`-c cpu-test.dig`): Sim 기본값 그대로.
- `-j N`: 동시 실행 수(기본 9). `-nobuild`: 프로그램을 다시 빌드하지 않음. `이름`을 주면 그 테스트만 돌립니다(기본 전체).
- 출력은 `program/build/dsim/<이름>.sim`, `.emu`입니다. 이름 끝이 `_nh`인 이미지는 `halt`를 `jmp.one pc.32B, 0`으로 바꿔 빌드한 것입니다.
- 테스트 목록은 스크립트의 `T` 배열입니다. `knownbug`는 알려진 문제를 담고 있어 DIFF가 정상입니다.
- `DIGITAL_JAR` 환경 변수로 `Digital.jar` 경로를 바꿀 수 있습니다.

## runint.py : 인터럽트 회귀 테스트

```
python tools/dsim/runint.py [-j N] [-seeds 1,2,3] [-nobuild] [그룹...]
```

- 에뮬레이터에 인터럽트가 없어서 `document/ISA.md` 2.2절로 계산한 값과 비교합니다. 항목과 최근 결과는 `program/document/test.md`에 있습니다.
- 그룹은 `ext prec sled misp iret sys tbl`이고(기본 전체), `-seeds`는 `prec` sweep의 zseed입니다(나머지는 첫 번째 값). 전체 약 100초입니다.
- 항목마다 `PASS`, `FAIL`, `XFAIL`, `XPASS`, `SKIP`을 출력하고, `FAIL`이나 `XPASS`가 있으면 종료 코드 1입니다. 지금 실패가 예상되는 항목은 스크립트의 `XFAIL`에 이유와 함께 있습니다.
- 회로 입력은 `-Dset=E=1,run=1,INT_vec=8,INT_lev=1`, `-Dat=H:INT_in=1;H+60:INT_in=0`입니다. 그래서 핸들러는 주소 8에 둡니다. H는 인터럽트 없는 실행에서 PC가 루프 주소에 닿는 half로 계산합니다.
- `components/`를 `program/build/dsim/int/components/`로 복사해서 돌리므로 실행 중 회로를 저장해도 됩니다. 출력은 같은 폴더의 `<프로그램>-z<seed>-<H>.sim`, `.delta`입니다.
- `bin/dasm.exe`가 `interst`/`interld`를 어셈블하지 못해 `int_tbl`은 nop을 패치합니다. `bin/dasm.exe`를 바꾸면 `int_tbl.asm`에 명령을 쓰고 스크립트의 `PATCH`를 지웁니다.

## NetQuery : 넷 조회

```
java -cp "$J;build" NetQuery <circuit.dig> net <넷이름>...       # 그 넷에 붙은 소자.핀
java -cp "$J;build" NetQuery <circuit.dig> at <x,y>...          # 그 위치 소자의 핀별 연결
java -cp "$J;build" NetQuery <circuit.dig> label <라벨>...
java -cp "$J;build" NetQuery <circuit.dig> sub ROB-SC.dig net D  # 하위 회로 안
```

### computer-32 실행

```
java -Ddip= -Didle=0 -Dset=E=0,run=0,boot=1 -Dat="10:E=1,boot=0,run=1" -Drom=<prog.bin> -cp "$J;build" Sim ../../components/computer-32.dig - "<keys>" <maxHalf>
```

- 최상위에 DipSwitch가 없으므로 `-Ddip=`를 줍니다(클럭 선택은 cpu-32 안에 켜진 채 저장됨).
- 최상위에 `PC`, `com1`, `com2`가 없어서 stall 검사가 꺼집니다. `maxHalf`나 `idle`로 끝내는데, selftest와 calc는 출력이 없는 구간이 40000 half보다 길어서 `idle`을 짧게 주면 중간에 끊깁니다.
- 2026-11-08에 memtest, memtest2, selftest(zseed 1), calc의 터미널 출력이 cpu-test와 같았습니다.

## sync-nondebug.sh : debug 회로를 non-debug 회로로 반영

```
sh sync-nondebug.sh [components 디렉터리]
```

- 디렉터리를 주지 않으면 스크립트 위치 기준 `../../components`를 씁니다. 처음에 실제 대상 경로를 출력합니다.
- debug 모듈은 `components/debugs/`에 있습니다. Digital은 연 회로의 폴더와 그 하위 폴더에서만 하위 회로를 찾으므로, `debugs/`의 모듈을 바로 열면 하위 회로를 잃습니다. 그래서 스크립트는 components를 임시 폴더에 복사하고 `debugs/*.dig`를 그 최상위로 옮긴 뒤 작업하고, 결과 6개만 되돌려 씁니다. 중간에 실패하면 아무 파일도 바꾸지 않습니다.
- `*-debug.dig`에서 non-debug 모듈을 다시 만듭니다(`id-manager`, `res-station`, `load-buffer`, `store-buffer`).
  - Probe와, probe에만 연결된 로직을 지웁니다.
  - id-manager에서는 디버그 Out `VV1`, `VV2`, `US`, `C16`도 지웁니다.
- `cpu-test.dig`에서 `cpu-32.dig`를 다시 만듭니다.
  - Probe, LED와 왼쪽(x < -1200)의 메모리/IO 영역을 지우고, 모듈 참조를 non-debug로 바꿉니다.
  - 키보드 값을 `D_D-_bus`에 올리는 Driver/Splitter(`KBD` = `box:-1200,1400,-1000,1540`)는 경계 오른쪽에 있지만 메모리/IO 쪽으로 보냅니다. IO 소자를 경계 오른쪽으로 더 옮기면 `KBD`처럼 예외를 추가해야 합니다(빠뜨리면 cpu-32 비교에서 `mismatched`로 나옵니다).
  - LED나 probe에만 연결된 로직은 남깁니다(`-Dnocascade=true`).
  - cpu-test에서는 메모리/IO 쪽이 직접 구동해서 연결되어 있지 않은 In 핀(`D_AMID`, `D_responce`, `I_responce`, `D_recieved`, `MMU_acp`, `MMU_phys`, `MMU_amid`)을 cpu-32에서만 내부 넷의 터널에 잇습니다(`fix_cpu32.py`). cpu-test에서 이 핀을 직접 연결하면 스크립트가 "already wired"로 멈추므로, 그 핀을 목록에서 빼면 됩니다. 새로 생긴 이런 핀은 computer-32를 열 때 `No output connected to a wire` 오류로 드러납니다.
- `cpu-test.dig`의 메모리/IO 영역(x < -1200, probe 제외)에 `cpu-32` 인스턴스를 붙여 `computer-32.dig`를 만듭니다.
  - `cpu-32`의 핀마다 핀 이름의 터널을 달아 메모리/IO 쪽 터널과 이름으로 연결합니다. 대소문자만 다른 이름(핀 `D_AMID`, `D_MID`, `MMU_md` ↔ 터널 `D_amid`, `D_mid`, `MMU_MD`)은 메모리/IO 쪽 이름을 씁니다(`pin A -> harness tunnel B`로 출력).
  - 메모리/IO가 쓰지 않는 핀(`E`, `run`, `boot`, `INT_*`, `MMU_*` 등)에는 `cpu-test`와 같은 설정의 In/Out을, `C`에는 같은 Clock을 답니다.
  - 하위 회로 안의 DipSwitch는 누를 수 없으므로, `cpu-32`의 클럭 선택 DipSwitch(-560,-1640)를 켠 상태로 저장합니다(`set_dip.py`).
- 마지막에 원본과 결과의 넷리스트를 비교합니다. `mismatched 0`이면 남은 모든 핀이 원본과 같은 핀들에 연결되어 있습니다. cpu-32는 `fix_cpu32.py` 전의 사본(`cpu-32-nofix`)으로 비교합니다.
  - harness 안의 wire로만 이어져 있던 터널(지금은 `MMU_req`→`MMU_acp` 루프백)은 cpu-32에서 일부러 갈라집니다. Strip이 `ALIAS-REMOVED MMU_acp|MMU_req`를 출력하고, 이 차이는 `mismatched`가 아니라 `expected split`으로 셉니다.
- 결과 파일을 바로 덮어쓰므로, 손으로 고친 non-debug 파일이 있으면 먼저 커밋해 두세요.

구성:

| 파일 | 하는 일 |
|---|---|
| `Strip.java` | `Strip <circuit.dig> <root>...` 지울 소자(`E name x y`)와 wire(`W x1 y1 x2 y2`)를 출력합니다. root는 `type:Probe`, `label:Out:VV1`, `left:-1200`, `right:-1200`, `box:x1,y1,x2,y2`(pos가 그 안, 경계 포함)이고, 앞에 `!`를 붙이면 앞의 root가 고른 소자를 다시 남깁니다(순서대로 적용). 쓸모없어진 터널과 wire도 함께 지웁니다. 이름이 다른 터널끼리 잇는 넷은 남기고, 그 연결이 지워지면 `ALIAS-REMOVED a\|b`를 출력합니다. |
| `strip_apply.py` | `strip_apply.py <in.dig> <list> <out.dig> [a.dig=b.dig ...]` 목록대로 XML에서 지우고 하위 회로 이름을 바꿉니다. 지우기 전에 XStream의 `reference="../visualElement[N]/..."`를 실제 값으로 풀어 씁니다(순번이 밀려 파일이 깨지는 것을 막음). |
| `NetSig.java` | `NetSig <circuit.dig>` 핀마다 넷 번호와 그 넷의 터널 이름(`a\|b`)을 출력합니다. |
| `netcmp.py` | `netcmp.py <a.sig> <b.sig> [-split a\|b ...]` 양쪽에 있는 핀끼리 넷 연결을 비교합니다. `-split`: 원본에서 터널 a, b가 붙은 넷이 핀을 잃는 것은 `expected split`으로 셉니다. |
| `make_computer.py` | `make_computer.py <harness.dig> <cpu-test.dig> <pins.txt> <out.dig>` 메모리/IO 회로에 `cpu-32` 인스턴스와 터널, In/Out을 붙입니다. |
| `fix_cpu32.py` | `fix_cpu32.py <cpu-32.dig> PIN=NET...` 연결되지 않은 In 핀에 그 방향으로 wire와 NET 터널을 붙입니다. |
| `set_dip.py` | `set_dip.py <circuit.dig> x,y...` 그 위치의 DipSwitch 기본값을 켭니다. |
| `Pins.java` | `Pins <아무 회로> <sub.dig>...` 하위 회로 shape의 핀 위치. 핀을 지운 뒤 다른 핀이 움직였는지 확인하는 용도입니다. |

이 회로는 Probe 하나만 지워도 실행 결과가 달라질 수 있으므로(race), 변환 결과는 실행 비교가 아니라 넷리스트 비교로 확인합니다.


## emutrace.py : 에뮬레이터 trace

```
python emutrace.py <program.bin> <keys> [시작주소hex] [-raw]
```

`program.bin`은 dlnk 기본 출력(8바이트 헤더: 시작 주소, 길이)이고, 헤더가 없는 이미지는 `-raw`를 붙입니다. opcode와 확장 레지스터 이름은 `tools/assembler/assembler.hpp`의 `instructionBase`, `regExBase`에서 읽습니다. 시작 주소부터 명령 140개를 주소, 워드, 역어셈블, SP와 함께 출력합니다.
