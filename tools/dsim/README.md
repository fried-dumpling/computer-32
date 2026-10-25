# dsim — Digital 회로 headless 시뮬레이터

`Digital.jar`(hneemann Digital 0.31)를 라이브러리로 써서 `components/cpu-test.dig`를 GUI 없이 돌립니다. 키 입력을 넣고, 터미널 출력을 받고, probe/tunnel 값을 반클럭마다 CSV로 덤프합니다. floating 버스 값이 무작위라서 같은 입력이어도 실행마다 결과가 달라질 수 있습니다. `-Dzseed=N`을 주면 같은 N에서 결과가 재현됩니다.

## 빌드

```
J="D:/Users/taeh5/Digital/Digital 0.31/Digital.jar"
javac -cp "$J" -d build Sim.java Patch.java NetQuery.java Strip.java NetSig.java Pins.java
```

## Sim : 실행

```
java [옵션] -cp "$J;build" Sim <circuit.dig> <program.bin> <keys> [maxHalfCycles]
```

- `keys`: 프로그램이 `> `를 출력할 때마다 한 글자씩 넣습니다. `-Dlines=1`이면 `|`로 나눈 한 줄씩 `\n`을 붙여 넣습니다(calc용).
- 옵션 (`-D이름=값`, `Sim` 앞에 씀)

| 옵션 | 뜻 |
|---|---|
| `dip=x,y;...` | 켤 DipSwitch 위치. 기본 `-580,-1640`(클럭 mux를 Clock 쪽으로) |
| `set=E=1,run=1` | top-level 입력 |
| `at=H:k=v,...;H:k=v` | 반클럭 H에 top-level 입력을 바꿉니다. 예: computer-32 시작 순서 `-Dset=E=0,run=0,boot=1 -Dat="10:E=1,boot=0,run=1"` (`document/cpu-32.md` §4) |
| `banner=text` | 리셋 때 한 번 출력되는 글자. 다시 보이면 재시작으로 판정합니다(기본 `m8 `) |
| `stopRestart=0` | 재시작해도 계속 실행 |
| `tap=top;ROB-SC.dig;...` | 해당 회로의 tunnel마다 probe 추가. 이름은 `T_<넷>`(top) 또는 `<파일>/<넷>` |
| `csv=파일` | 모든 signal(probe, In/Out)을 반클럭마다 덤프. `csvFrom=N`이나 `csvKey=N`(N번째 키부터) |
| `patch=scsfd` | `Patch.java`의 메모리 안 회로 수정 (파일은 그대로) |
| `lines=1` | 줄 단위 입력 |
| `zseed=N` / `zseed=zero` | floating(high-Z) 비트를 읽을 때 쓰는 난수를 고정합니다. Digital은 값마다 `new Random()`을 써서 읽을 때마다 다른 값을 주므로, 이 옵션이 없으면 실행마다 결과가 달라질 수 있습니다. `zero`면 항상 0 |
| `hash=파일` | 반클럭마다 모든 signal의 해시 한 줄. 두 실행이 처음 갈라지는 반클럭을 찾는 용도 |
| `delta=파일` | `csv`처럼 덤프하되 바뀐 값만 `half 열번호 값`으로 씁니다(첫 줄은 이름 목록) |

예 (memtest8 재현, 덤프):

```
java -Dtap=top -Dcsv=t3.csv -DcsvKey=3 -cp "$J;build" Sim "../../components/cpu-test.dig" run.txt "123"
```

## NetQuery : 넷 조회

```
java -cp "$J;build" NetQuery <circuit.dig> net <넷이름>...       # 그 넷에 붙은 소자.핀
java -cp "$J;build" NetQuery <circuit.dig> at <x,y>...          # 그 위치 소자의 핀별 연결
java -cp "$J;build" NetQuery <circuit.dig> label <라벨>...
java -cp "$J;build" NetQuery <circuit.dig> sub ROB-SC.dig net D  # 하위 회로 안
```

## sync-nondebug.sh : debug 회로를 non-debug 회로로 반영

```
sh sync-nondebug.sh
```

- `*-debug.dig`에서 non-debug 모듈을 다시 만듭니다(`id-manager`, `res-station`, `load-buffer`, `store-buffer`).
  - Probe와, probe에만 연결된 로직을 지웁니다.
  - id-manager에서는 디버그 Out `VV1`, `VV2`, `US`, `C16`도 지웁니다.
- `cpu-test.dig`에서 `cpu-32.dig`를 다시 만듭니다.
  - Probe, LED와 왼쪽(x < -1200)의 메모리/IO 영역을 지우고, 모듈 참조를 non-debug로 바꿉니다.
  - LED나 probe에만 연결된 로직은 남깁니다(`-Dnocascade=true`).
- `cpu-test.dig`의 메모리/IO 영역(x < -1200, probe 제외)에 `cpu-32` 인스턴스를 붙여 `computer-32.dig`를 만듭니다.
  - `cpu-32`의 핀마다 핀 이름의 터널을 달아 메모리/IO 쪽 터널과 이름으로 연결합니다.
  - 메모리/IO가 쓰지 않는 핀(`E`, `run`, `boot`, `INT_*`, `MMU_*` 등)에는 `cpu-test`와 같은 설정의 In/Out을, `C`에는 같은 Clock을 답니다.
  - 하위 회로 안의 DipSwitch는 누를 수 없으므로, `cpu-32`의 클럭 선택 DipSwitch(-580,-1640)를 켠 상태로 저장합니다(`set_dip.py`).
- 마지막에 원본과 결과의 넷리스트를 비교합니다. `mismatched 0`이면 남은 모든 핀이 원본과 같은 핀들에 연결되어 있습니다.
- 결과 파일을 바로 덮어쓰므로, 손으로 고친 non-debug 파일이 있으면 먼저 커밋해 두세요.

구성:

| 파일 | 하는 일 |
|---|---|
| `Strip.java` | `Strip <circuit.dig> <root>...` 지울 소자(`E name x y`)와 wire(`W x1 y1 x2 y2`)를 출력합니다. root는 `type:Probe`, `label:Out:VV1`, `left:-1200`, `right:-1200`입니다. 쓸모없어진 터널과 wire도 함께 지웁니다. 이름이 다른 터널끼리 잇는 넷은 남깁니다. |
| `strip_apply.py` | `strip_apply.py <in.dig> <list> <out.dig> [a.dig=b.dig ...]` 목록대로 XML에서 지우고 하위 회로 이름을 바꿉니다. 지우기 전에 XStream의 `reference="../visualElement[N]/..."`를 실제 값으로 풀어 씁니다(순번이 밀려 파일이 깨지는 것을 막음). |
| `NetSig.java` | `NetSig <circuit.dig>` 핀마다 넷 번호를 출력합니다. |
| `netcmp.py` | `netcmp.py <a.sig> <b.sig>` 양쪽에 있는 핀끼리 넷 연결을 비교합니다. |
| `make_computer.py` | `make_computer.py <harness.dig> <cpu-test.dig> <pins.txt> <out.dig>` 메모리/IO 회로에 `cpu-32` 인스턴스와 터널, In/Out을 붙입니다. |
| `set_dip.py` | `set_dip.py <circuit.dig> x,y...` 그 위치의 DipSwitch 기본값을 켭니다. |
| `Pins.java` | `Pins <아무 회로> <sub.dig>...` 하위 회로 shape의 핀 위치. 핀을 지운 뒤 다른 핀이 움직였는지 확인하는 용도입니다. |

이 회로는 Probe 하나만 지워도 실행 결과가 달라질 수 있으므로(race), 변환 결과는 실행 비교가 아니라 넷리스트 비교로 확인합니다.

## Patch : 실험용 회로 수정

- `scsfd`: `scs_i = clr ? FD : SCS±N`을 `(clr ? FD : SCS)±N`으로 바꿉니다. flush 반클럭에 dispatch된 call/ret의 SP 갱신이 버려지는 문제(원인 3)를 고치는지 확인하는 용도입니다. 회로에 이미 반영됐으므로, 현재 회로에서는 대상 mux가 없어서 실패합니다.
- `wfprio`: RS의 FL/SC 의존 태그 갱신에서 실제 삽입을 commit-clear보다 우선시킵니다(원인 4). `inst-wfile`의 FL/SC data mux와 RS의 `FL_inst`/`SC_inst` mux를 바꿉니다.

`-Dprobe=res-station-debug.dig@x,y=이름;...`으로 이름 없는 넷 좌표에도 probe를 달 수 있습니다.

## emutrace.py : 에뮬레이터 trace

```
python emutrace.py <program.bin> <keys> [시작주소hex]
```

시작 주소부터 명령 140개를 주소, 워드, 역어셈블, SP와 함께 출력합니다.
