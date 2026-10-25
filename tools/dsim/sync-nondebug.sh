#!/bin/sh
# Regenerate the non-debug modules and cpu-32.dig from the debug modules and cpu-test.dig,
# then check that every remaining pin is on the same net as in the source.
#   sh sync-nondebug.sh            (from tools/dsim, after building)
# Writes components/{id-manager,res-station,load-buffer,store-buffer,cpu-32,computer-32}.dig in place.
set -e
J="${J:-D:/Users/taeh5/Digital/Digital 0.31/Digital.jar}"
HERE="$(pwd)"
# java on Windows needs a D:/... path (pwd -W in git bash)
CP="$J;$(pwd -W 2>/dev/null || pwd)/build"
T="$(mktemp -d)"
cd ../../components

for m in res-station load-buffer store-buffer id-manager; do
    extra=""
    # debug-only outputs of id-manager (only probes in cpu-test.dig use them)
    [ $m = id-manager ] && extra="label:Out:VV1 label:Out:VV2 label:Out:US label:Out:C16"
    java -cp "$CP" Strip $m-debug.dig type:Probe $extra 2>/dev/null > "$T/$m.list"
    python "$HERE/strip_apply.py" $m-debug.dig "$T/$m.list" $m.dig
done

# cpu-32 = cpu-test without probes, LEDs and the memory/IO harness (x < -1200).
# nocascade: logic that only feeds LEDs/probes stays (it may be unfinished wiring).
java -Dnocascade=true -cp "$CP" Strip cpu-test.dig type:Probe type:LED left:-1200 2>/dev/null > "$T/cpu.list"
grep -v '^[EW] ' "$T/cpu.list" || true
python "$HERE/strip_apply.py" cpu-test.dig "$T/cpu.list" cpu-32.dig \
    id-manager-debug.dig=id-manager.dig res-station-debug.dig=res-station.dig \
    load-buffer-debug.dig=load-buffer.dig store-buffer-debug.dig=store-buffer.dig
# DipSwitches in an embedded circuit cannot be clicked: turn on the clock mux select (-580,-1640, Sim's
# default dip) so cpu-32 runs from its C input
python "$HERE/set_dip.py" cpu-32.dig -580,-1640

# computer-32 = the memory/IO harness of cpu-test (x < -1200) + a cpu-32 instance, joined by tunnels
java -Dnocascade=true -cp "$CP" Strip cpu-test.dig type:Probe right:-1200 2>/dev/null > "$T/harness.list"
python "$HERE/strip_apply.py" cpu-test.dig "$T/harness.list" "$T/harness.dig"
java -cp "$CP" Pins cpu-test.dig cpu-32.dig 2>/dev/null > "$T/pins.txt"
python "$HERE/make_computer.py" "$T/harness.dig" cpu-test.dig "$T/pins.txt" computer-32.dig

for p in res-station-debug:res-station load-buffer-debug:load-buffer store-buffer-debug:store-buffer \
         id-manager-debug:id-manager cpu-test:cpu-32 cpu-test:computer-32; do
    a=${p%%:*}; b=${p##*:}
    java -cp "$CP" NetSig $a.dig 2>/dev/null > "$T/a.sig"
    java -cp "$CP" NetSig $b.dig 2>/dev/null > "$T/b.sig"
    echo "## $a -> $b"
    python "$HERE/netcmp.py" "$T/a.sig" "$T/b.sig"
done
rm -rf "$T"
