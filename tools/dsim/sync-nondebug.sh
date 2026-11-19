#!/bin/sh
# Regenerate the non-debug modules and cpu-32.dig from the debug modules and cpu-test.dig,
# then check that every remaining pin is on the same net as in the source.
#   sh sync-nondebug.sh [components dir]   (after building)
# Reads <dir>/debugs/*-debug.dig and <dir>/cpu-test.dig, writes <dir>/{id-manager,res-station,load-buffer,store-buffer,cpu-32,computer-32}.dig in place.
# Digital finds subcircuits only in the opened file's folder and below, so a module in debugs/ cannot be
# opened on its own: work in a temp copy of <dir> with debugs/*.dig moved to the top.
# dir defaults to ../../components relative to this script.
set -e
J="${J:-D:/Users/taeh5/Digital/Digital 0.31/Digital.jar}"
HERE="$(cd "$(dirname "$0")" && pwd)"
# java on Windows needs a D:/... path (pwd -W in git bash)
CP="$J;$(cd "$HERE" && (pwd -W 2>/dev/null || pwd))/build"
cd "${1:-$HERE/../../components}"
SRC="$(pwd)"
echo "components: $SRC"
T="$(mktemp -d)"
cp -r . "$T/c"
mv "$T/c"/debugs/*.dig "$T/c"/
rm -rf "$T/c/debugs"
cd "$T/c"

for m in res-station load-buffer store-buffer id-manager; do
    extra=""
    # debug-only outputs of id-manager (only probes in cpu-test.dig use them)
    [ $m = id-manager ] && extra="label:Out:VV1 label:Out:VV2 label:Out:US label:Out:C16"
    java -cp "$CP" Strip $m-debug.dig type:Probe $extra 2>/dev/null > "$T/$m.list"
    python "$HERE/strip_apply.py" $m-debug.dig "$T/$m.list" $m.dig
done

# cpu-32 = cpu-test without probes, LEDs and the memory/IO harness (x < -1200, plus KBD: the keyboard
# drivers onto D_D-_bus, which sit right of the line).
# nocascade: logic that only feeds LEDs/probes stays (it may be unfinished wiring).
KBD="box:-1200,1400,-1000,1540"
java -Dnocascade=true -cp "$CP" Strip cpu-test.dig type:Probe type:LED left:-1200 $KBD 2>/dev/null > "$T/cpu.list"
grep -v '^[EW] ' "$T/cpu.list" || true
python "$HERE/strip_apply.py" cpu-test.dig "$T/cpu.list" cpu-32.dig \
    id-manager-debug.dig=id-manager.dig res-station-debug.dig=res-station.dig \
    load-buffer-debug.dig=load-buffer.dig store-buffer-debug.dig=store-buffer.dig
# DipSwitches in an embedded circuit cannot be clicked: turn on the clock mux select (-560,-1640, Sim's
# default dip) so cpu-32 runs from its C input
python "$HERE/set_dip.py" cpu-32.dig -560,-1640
# cpu-test feeds these nets from its harness and leaves the In pins unconnected: join them in cpu-32
# (the netlist check below uses the copy from before this step)
cp cpu-32.dig cpu-32-nofix.dig
python "$HERE/fix_cpu32.py" cpu-32.dig D_AMID=D_amid D_responce=D_responce I_responce=I_responce D_recieved=D_recieved \
    MMU_acp=MMU_acp MMU_phys=MMU_phys MMU_amid=MMU_amid

# computer-32 = the memory/IO harness of cpu-test (x < -1200) + a cpu-32 instance, joined by tunnels
java -Dnocascade=true -cp "$CP" Strip cpu-test.dig type:Probe right:-1200 "!$KBD" 2>/dev/null > "$T/harness.list"
python "$HERE/strip_apply.py" cpu-test.dig "$T/harness.list" "$T/harness.dig"
java -cp "$CP" Pins cpu-test.dig cpu-32.dig 2>/dev/null > "$T/pins.txt"
python "$HERE/make_computer.py" "$T/harness.dig" cpu-test.dig "$T/pins.txt" computer-32.dig

for p in res-station-debug:res-station load-buffer-debug:load-buffer store-buffer-debug:store-buffer \
         id-manager-debug:id-manager cpu-test:cpu-32-nofix cpu-test:computer-32; do
    a=${p%%:*}; b=${p##*:}
    java -cp "$CP" NetSig $a.dig 2>/dev/null > "$T/a.sig"
    java -cp "$CP" NetSig $b.dig 2>/dev/null > "$T/b.sig"
    echo "## $a -> $b"
    # nets joined only by a wire in the stripped harness (Strip ALIAS-REMOVED) split in cpu-32 on purpose
    split=""
    [ $b = cpu-32-nofix ] && split="$(sed -n 's/^ALIAS-REMOVED /-split /p' "$T/cpu.list" | tr '\n' ' ')"
    python "$HERE/netcmp.py" "$T/a.sig" "$T/b.sig" $split
done
cp id-manager.dig res-station.dig load-buffer.dig store-buffer.dig cpu-32.dig computer-32.dig "$SRC"/
cd "$SRC"
rm -rf "$T"
