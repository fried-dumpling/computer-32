# dsim — Digital 회로 headless 시뮬레이터

`Digital.jar`(hneemann Digital 0.31)를 라이브러리로 써서 `components/cpu-32 test.dig`를 GUI 없이 돌립니다. 키 입력을 넣고, 터미널 출력을 받고, probe/tunnel 값을 반클럭마다 CSV로 덤프합니다. 같은 입력이면 결과가 거의 항상 같습니다. 다만 드물게 실행마다 달라지는 race가 있습니다(`log/compare/deadlock-investigation.md` "원인 3 > 남은 문제").

## 빌드

```
J="D:/Users/taeh5/Digital/Digital 0.31/Digital.jar"
javac -cp "$J" -d build Sim.java Patch.java NetQuery.java
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
| `banner=text` | 리셋 때 한 번 출력되는 글자. 다시 보이면 재시작으로 판정합니다(기본 `m8 `) |
| `stopRestart=0` | 재시작해도 계속 실행 |
| `tap=top;ROB-SC.dig;...` | 해당 회로의 tunnel마다 probe 추가. 이름은 `T_<넷>`(top) 또는 `<파일>/<넷>` |
| `csv=파일` | 모든 signal(probe, In/Out)을 반클럭마다 덤프. `csvFrom=N`이나 `csvKey=N`(N번째 키부터) |
| `patch=scsfd` | `Patch.java`의 메모리 안 회로 수정 (파일은 그대로) |
| `lines=1` | 줄 단위 입력 |

예 (memtest8 재현, 덤프):

```
java -Dtap=top -Dcsv=t3.csv -DcsvKey=3 -cp "$J;build" Sim "../../components/cpu-32 test.dig" run.txt "123"
```

## NetQuery : 넷 조회

```
java -cp "$J;build" NetQuery <circuit.dig> net <넷이름>...       # 그 넷에 붙은 소자.핀
java -cp "$J;build" NetQuery <circuit.dig> at <x,y>...          # 그 위치 소자의 핀별 연결
java -cp "$J;build" NetQuery <circuit.dig> label <라벨>...
java -cp "$J;build" NetQuery <circuit.dig> sub ROB-SC.dig net D  # 하위 회로 안
```

## Patch : 실험용 회로 수정

- `scsfd`: `scs_i = clr ? FD : SCS±N`을 `(clr ? FD : SCS)±N`으로 바꿉니다. flush 반클럭에 dispatch된 call/ret의 SP 갱신이 버려지는 문제(원인 3)를 고치는지 확인하는 용도입니다. 회로에 이미 반영됐으므로, 현재 회로에서는 대상 mux가 없어서 실패합니다.
- `wfprio`: RS의 FL/SC 의존 태그 갱신에서 실제 삽입을 commit-clear보다 우선시킵니다(원인 4). `inst-wfile`의 FL/SC data mux와 RS의 `FL_inst`/`SC_inst` mux를 바꿉니다.

`-Dprobe=res-station-debug.dig@x,y=이름;...`으로 이름 없는 넷 좌표에도 probe를 달 수 있습니다.

## emutrace.py : 에뮬레이터 trace

```
python emutrace.py <program.bin> <keys> [시작주소hex]
```

시작 주소부터 명령 140개를 주소, 워드, 역어셈블, SP와 함께 출력합니다.
