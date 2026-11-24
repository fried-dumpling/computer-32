# dsim — Digital 회로 headless 시뮬레이터

`Digital.jar`(hneemann Digital 0.31)를 라이브러리로 써서 CPU 회로(`components/computer-32-test.dig`)를 GUI 없이 돌립니다. 키 입력을 넣고, 터미널 출력을 받고, probe/tunnel 값을 반클럭마다 CSV로 덤프합니다. floating 버스 값이 무작위라서 같은 입력이어도 실행마다 결과가 달라질 수 있습니다. `-Dzseed=N`을 주면 같은 N에서 결과가 재현됩니다.

## 빌드

```
J="D:/Users/taeh5/Digital/Digital 0.31/Digital.jar"
javac -cp "$J" -d build Sim.java NetQuery.java NetSig.java Pins.java
```

## Sim : 실행

```
java [옵션] -cp "$J;build" Sim <circuit.dig> <program.bin> <keys> [maxHalfCycles]
```

- `keys`: 프로그램이 `> `를 출력할 때마다 한 글자씩 넣습니다. `-Dlines=1`이면 `|`로 나눈 한 줄씩 `\n`을 붙여 넣습니다(calc용). 키를 다 쓴 뒤 다음 `> `가 나오면 끝냅니다(`""`이면 첫 `> `에서 끝).
- 끝날 때 `[half=... keysFed=... PC=... stop: 이유]`를 출력합니다. 이유는 `keys used up`, `restart`, `stall N`, `idle N`, `maxOut N`, `maxRepeat N`, `ERROR <메시지>`이고, `maxHalfCycles`(기본 2000000)까지 돌았으면 비어 있습니다.
- `ERROR`: Digital이 시뮬레이션 중 오류(예: `BurnException` "More than one output is active on a wire", 단락)를 내면 Model이 닫히고 그 반클럭에서 멈춥니다. stderr에 `DIGITAL ERROR at half H: ...`를, 단락이면 `origin:`(.dig 파일)과 그 net에 걸린 값(`value:`)도 출력합니다. 어느 net인지 알 수 있는 단서는 이것뿐입니다.
- 옵션 (`-D이름=값`, `Sim` 앞에 씀)

| 옵션 | 뜻 |
|---|---|
| `dip=x,y;f.dig@x,y;...` | 켤 DipSwitch 위치. `x,y`는 top, `f.dig@x,y`는 하위 회로 `f` 안. 기본 `-560,-1640`(예전 `cpu-test.dig`의 클럭 mux. `computer-32-test.dig`에서는 `-Ddip=-780,580;-780,620`(E, run)을 줍니다), `dip=`이면 하나도 켜지 않습니다. 그 위치에 DipSwitch가 없으면 에러로 멈춥니다(옮겨진 스위치 때문에 클럭 없이 아무 일도 안 하는 실행을 막음) |
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
java "-Ddip=-780,580;-780,620" -Dset= -Dtap=cpu-32.dig:PCS=PC,com1,com2 -Dcsv=t3.csv -DcsvKey=3 -cp "$J;build" Sim "../../components/computer-32-test.dig" - "123"
```

### 상주 모드 (`-server`)

```
java -cp "$J;build" Sim -server
```

- JVM을 띄운 채로 stdin 한 줄마다 실행을 한 번 합니다: `RUN<TAB>출력파일<TAB>옵션=값<TAB>...<TAB>--<TAB>회로<TAB>프로그램<TAB>키<TAB>maxHalf`(옵션은 위 `-D` 옵션에서 `-D`를 뺀 것). 그 실행의 stdout은 출력 파일로 가고, stdout에는 `OK`나 `ERR <메시지>`를 답합니다(stack trace는 stderr).
- 불러온 회로는 회로 파일, 그 mtime, `dip`/`tap`/`probe`/`pinprobe`가 같으면 재사용합니다. 실행마다 새 model을 만들므로 이전 실행의 상태는 남지 않습니다(같은 `zseed`면 한 번씩 띄운 실행과 출력이 같음, 2026-11-19 확인).
- `maxOut=N`, `maxRepeat=N`, `limitSkip=문자들`: 출력이 N자가 되거나 같은 글자가 N번 이어지면 멈춥니다(`limitSkip`의 글자는 세지 않음). 다시 시작하거나 한 핸들러를 계속 도는 실행을 일찍 끝내는 용도입니다.
- 실행마다 stderr에 `[time model=.. seed=.. sim=.. halves=..]`를 씁니다. `computer-32-test.dig`에서 JVM 시작 + 회로 로딩이 1회 약 3-5초, 이후 model 생성은 0.1초 정도입니다.

## runtests.sh : 회귀 테스트

```
bash tools/dsim/runtests.sh [-j N] [-nobuild] [-c 회로.dig] [이름...]
```

- 테스트 프로그램을 빌드해 `components/<회로>`에서 병렬로 돌리고(`-Drom`), 터미널 출력을 에뮬레이터와 비교해 `== 이름 SAME|DIFF|ERROR`를 출력하고, 끝에 `summary: SAME=.. DIFF=..`를 출력합니다. `ERROR`(Sim이 `stop: ERROR`로 멈춤)가 하나라도 있으면 exit 1입니다. 비교 전에 Sim의 stderr 줄(`[time ...]`은 마지막 출력과 같은 줄에 붙을 수 있어 그 부분만 잘라냄)을 지웁니다. git-bash에서 실행하고, `build/`에 Sim이 컴파일되어 있어야 합니다.
- 회로는 기본 `computer-32-test.dig`(2026-11-08부터, `DSIM_CIRCUIT` 환경 변수나 `-c`로 바꿈)입니다. 회로별 Sim 옵션은 스크립트의 `COPT`입니다.
  - `computer-32-test.dig`: top의 DipSwitch `-780,580`(E), `-780,620`(run)을 켜고 `-Dset=`. `cpu-32`에는 probe가 없으므로 stall 검사용으로 `-Dtap=cpu-32.dig:PCS=PC,com1,com2`. 다른 신호는 `-Dtap=cpu-32.dig:넷,...`이나 `-Dprobe=cpu-32.dig@x,y=이름`으로 그때마다 붙입니다.
- `-j N`: 동시 실행 수(기본 9). `-nobuild`: 프로그램을 다시 빌드하지 않음. `이름`을 주면 그 테스트만 돌립니다(기본 전체).
- 출력은 `program/build/dsim/<이름>.sim`, `.emu`입니다. 이름 끝이 `_nh`인 이미지는 `halt`를 `jmp.one pc.32B, 0`으로 바꿔 빌드한 것입니다.
- 테스트 목록은 스크립트의 `T` 배열입니다. `knownbug`는 알려진 문제를 담고 있어 DIFF가 정상입니다.
- `DIGITAL_JAR` 환경 변수로 `Digital.jar` 경로를 바꿀 수 있습니다.

## inttest.py : 인터럽트 회귀 테스트

```
python tools/dsim/inttest.py [-j N] [-seeds 1,2,3] [-nobuild] [-snap] [-full] [-nolimit] [-stall N] [-oneshot] [그룹...]
```

- `computer-32-test.dig`(CPU + IPU)에서 `program/test/interrupt/int_*.asm`을 돌려 `document/ISA.md` 2.2절로 계산한 값과 비교합니다(에뮬레이터에 인터럽트가 없음). 항목, 방법, 최근 결과는 `program/document/test.md`에 있습니다.
- 인터럽트는 top-level In `I-in`의 bit `IRQ`(3)에 넣는 2 half 펄스입니다(`-Dat=H:I-in=8;H+2:I-in=0`). 시작은 boot-sequencer S/R입니다.
- 지금 실패가 예상되는 항목은 스크립트의 `XFAIL`에 이유와 함께 있습니다.
- Digital 오류(`stop: ERROR`, 단락 등)로 끝난 실행은 항목 0 `ERROR`로 요약에 나오고 exit 1입니다. 자세한 내용은 그 실행의 `.sim`과 worker 로그에 있습니다.
- 그룹 하나가 예외로 멈추면(예: 보정 실행 출력이 `L<주소>...`로 시작하지 않음) 그 그룹만 항목 0 `FAIL group <그룹> stopped`로 남기고 다음 그룹을 계속 돌립니다(예전에는 전체가 `stopped:`로 끝났음).
- 스레드마다 `Sim -server`를 하나 띄워 두고 실행을 보냅니다(worker 로그 `program/build/dsim/int/worker-N.log`). `-oneshot`은 예전처럼 실행마다 java를 새로 띄웁니다. `iret2` + `prec`(231회, `-j 9`) 기준 255초에서 59초로 줄었고 결과는 같았습니다(2026-11-19).
- 같은 `program/build/dsim/int`에서 두 개를 동시에 돌리면 서로 `.sim` 파일과 회로 복사본을 덮어쓰므로, 시작할 때 `inttest.lock`(pid)을 만들고 다른 실행이 살아 있으면 멈춥니다. 강제로 끝난 실행이 남긴 lock은 pid가 없으면 지웁니다. 다른 폴더에서 돌리려면 `INTTEST_OUT=int-x`(`program/build/dsim/` 아래 이름)를 줍니다.
- 기본 실행은 2026-11-19부터 다음처럼 줄였습니다. 같은 회로 복사본에서 예전 방식(`-full -nolimit -stall 6000`, 약 1,650회, `-j 6` 1770초)과 비교해 45개 항목의 결과가 모두 같았고 1,079회에 231초가 걸렸습니다(`sysirq` 630→377, `nest` 492→247, `iret2` 189→115).
  - `sysirq`, `nest`, `iret2`: 이벤트 근처만 2 half마다, 나머지는 6 half마다(16 half 루프의 모든 위상에 걸림) 첫 seed에서만 펄스를 넣습니다. 이벤트는 `sysirq`가 루프 시작, INT_req 상승, 그 뒤 60 half(±40), `nest`가 진입, `open`, `close`, `close`+16(±24), `iret2`가 iret fetch(±24)이고, `iret2`는 첫 seed에서 범위 전체를 돕니다. 비교 실행 1,500회에서 seed가 다르다고 결과가 달라진 펄스 위치는 없었습니다. `-full`은 예전처럼 모든 위치, 모든 seed입니다.
  - 조기 종료(`-nolimit`로 끔): 다시 시작하는 프로그램(`L`이 두 번, Sim `banner`), `.`을 뺀 출력이 1000자(`maxOut`), 같은 글자 100번(`maxRepeat`, `limitSkip=.`). 정상 출력은 210자 이하이고 `L`을 한 번만 찍습니다. 전에는 이런 실행이 400000 half(약 20초)까지 돌았습니다.
  - `-stall` 기본 2000(전에는 6000). 정상 실행에서 PC가 가장 오래 그대로였던 구간은 36 half입니다.

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
- 2026-11-08에 memtest, memtest2, selftest(zseed 1), calc의 터미널 출력이 cpu-test(지금 `legacy/`)와 같았습니다.

## 넷 조사 도구

| 파일 | 하는 일 |
|---|---|
| `NetSig.java` | `NetSig <circuit.dig>` 핀마다 넷 번호와 그 넷의 터널 이름(`a\|b`)을 출력합니다. |
| `Pins.java` | `Pins <아무 회로> <sub.dig>...` 하위 회로 shape의 핀 위치. 핀을 지운 뒤 다른 핀이 움직였는지 확인하는 용도입니다. |

debug 모듈과 `cpu-test.dig`에서 non-debug 모듈, `cpu-32.dig`, `computer-32.dig`를 다시 만들던 `sync-nondebug.sh`와 그 도구(`Strip`, `strip_apply.py`, `make_computer.py`, `fix_cpu32.py`, `set_dip.py`, `netcmp.py`)는 2026-11-19에 지웠습니다. 지금은 `cpu-32.dig`를 직접 고치고, `cpu-test.dig`는 `components/legacy/`에 있습니다. 필요하면 git 기록에서 꺼냅니다.


## emutrace.py : 에뮬레이터 trace

```
python emutrace.py <program.bin> <keys> [시작주소hex] [-raw]
```

`program.bin`은 dlnk 기본 출력(8바이트 헤더: 시작 주소, 길이)이고, 헤더가 없는 이미지는 `-raw`를 붙입니다. opcode와 확장 레지스터 이름은 `tools/assembler/assembler.hpp`의 `instructionBase`, `regExBase`에서 읽습니다. 시작 주소부터 명령 140개를 주소, 워드, 역어셈블, SP와 함께 출력합니다.
