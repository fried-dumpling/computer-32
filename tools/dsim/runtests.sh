#!/bin/bash
# runtests.sh [-j N] [-nobuild] [name...]
#   Builds the test programs, runs them on components/cpu-test.dig in parallel (each with -Drom=...,
#   so boot/rom.bin is not touched) and compares the terminal output with the emulator.
#   name : only these tests (default all). Output: program/build/dsim/<name>.sim / .emu
#   Run from git-bash. Needs Sim compiled into build/ (see README.md).
cd "$(dirname "$0")"
D=$(pwd -W)                               # tools/dsim (Windows path, for java)
R=$(cd ../.. && pwd -W)
P="$R/program"; O="$P/build/dsim"; mkdir -p "$O"
J=${DIGITAL_JAR:-"D:/Users/taeh5/Digital/Digital 0.31/Digital.jar"}
JOBS=9; BUILD=1
while [ $# -gt 0 ]; do
  case $1 in
    -j) JOBS=$2; shift 2 ;;
    -nobuild) BUILD=0; shift ;;
    *) break ;;
  esac
done

# name|bin|zseed|keys (Sim, '|' between lines)|emulator keys ('\n' between lines)|Sim options
T=(
"selftest-z1|selftest|1|||"
"selftest-z2|selftest|2|||"
"selftest-z3|selftest|3|||"
"memtest|memtest|1|||"
"memtest2|memtest2|1|||"
"calc-halt|calc|1|1+2|q|1+2\nq\n|-Dlines=1"
"calc|calc_nh|1|1+2|2*3-1|1/4|q|1+2\n2*3-1\n1/4\nq\n|-Dlines=1"
"picalc|pi_nh|1|0 20|10 10|q|0 20\n10 10\nq\n|-Dlines=1"
"fill|fill_nh|1|||"
"flushstall|flushstall|1|||"
"ldstmix|ldstmix|1|||"
"knownbug|knownbug|1|||"     # DIFF expected: the known problems in CLAUDE.md
"rotate|rotate|1|||"
)

# asm main.asm out.bin [lib.asm...] : main object first. *_nh: halt -> jmp.one pc.32B, 0
# (halt runs before an older branch resolves, see CLAUDE.md). The copy sits next to the source
# because #include paths are relative to it.
asm() {
  local src=$1 out=$2; shift 2
  local m=$src objs=() s
  if [[ $out == *_nh.bin ]]; then
    m="${src%.asm}.nh.tmp.asm"
    sed -E 's/^([[:space:]]*)halt\b/\1jmp.one pc.32B, 0/' "$src" > "$m"
  fi
  for s in "$m" "$@"; do
    local o="$O/$(basename "${s%.asm}").o"
    "$R/bin/dasm.exe" "$s" -o "$o" > "$O/build.log" 2>&1 || { cat "$O/build.log"; echo "dasm failed: $s"; exit 1; }
    objs+=("$o")
  done
  [ "$m" != "$src" ] && rm -f "$m"
  "$R/bin/dlnk.exe" "${objs[@]}" -o "$O/$out" > "$O/build.log" 2>&1 || { cat "$O/build.log"; echo "dlnk failed: $out"; exit 1; }
}
if [ $BUILD = 1 ]; then
  asm "$P/test/selftest.asm" selftest.bin "$P/floatingpoint/float.asm"
  asm "$P/test/memtest.asm" memtest.bin
  asm "$P/test/memtest2.asm" memtest2.bin
  asm "$P/test/fill.asm" fill_nh.bin
  asm "$P/test/flushstall.asm" flushstall.bin
  asm "$P/test/ldstmix.asm" ldstmix.bin
  asm "$P/test/knownbug.asm" knownbug.bin
  asm "$P/test/rotate.asm" rotate.bin
  asm "$P/calc/calc.asm" calc.bin "$P/floatingpoint/float.asm" "$P/io/io.asm"
  asm "$P/calc/calc.asm" calc_nh.bin "$P/floatingpoint/float.asm" "$P/io/io.asm"
  asm "$P/picalc/pi.asm" pi_nh.bin "$P/io/io.asm"
fi

run() { # name bin seed keys emukeys options
  local n=$1 b=$2 z=$3 k=$4 ek=$5 x=$6 r
  timeout 900 java -Dzseed=$z -Drom="$O/$b.bin" $x -cp "$J;$D/build" Sim "$R/components/cpu-test.dig" - "$k" 4000000 2>&1 \
    | grep -v -E "WARNING|Importer|zseed" > "$O/$n.sim"
  python "$R/tools/emu/isa32emu.py" "$O/$b.bin" "$(printf "$ek")" > "$O/$n.emu" 2>&1
  if diff <(grep -v '^\[half' "$O/$n.sim" | sed '/^$/d') <(sed -e '/^\[/,$d' -e '/^$/d' "$O/$n.emu") > /dev/null
  then r=SAME; else r=DIFF; fi
  echo "== $n $r $(tail -1 "$O/$n.sim")"
}
for t in "${T[@]}"; do
  # keys may contain '|', so split the name, bin, seed from the front and options, emulator keys from the back
  n=${t%%|*}; r=${t#*|}; b=${r%%|*}; r=${r#*|}; z=${r%%|*}; r=${r#*|}
  x=${r##*|}; r=${r%|*}; ek=${r##*|}; k=${r%|*}
  [ $# -gt 0 ] && [[ ! " $* " =~ " $n " ]] && continue
  while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
  run "$n" "$b" "$z" "$k" "$ek" "$x" &
done
wait
