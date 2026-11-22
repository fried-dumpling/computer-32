#!/usr/bin/env python
"""Interrupt regression tests on components/computer-32-test.dig (CPU + IPU).

  python tools/dsim/inttest.py [-j N] [-seeds 1,2,3] [-nobuild] [-snap] [-full] [-nolimit] [-stall N]
                               [-oneshot] [group...]
  group : ext prec sled misp flc iret iret2 sys sysprev sysirq nest wpsys sysmem user sys0 tbl
          (default all). Output: program/build/dsim/int/

The emulator has no interrupts, so each item is compared with values from ISA.md 2.2, not
with the emulator. An item in XFAIL is expected to fail today: it prints XFAIL while it
fails and XPASS once it passes. The exit code is 1 on FAIL or XPASS (then remove the entry)
or ERROR (item 0: a run that a Digital error such as a short circuit ended, "stop: ERROR").

An interrupt is a 2-half pulse on IPU line IRQ (top-level In "I-in"). The programs set
int_vec[IRQ] = handler and int_pri[IRQ] = 1 with INTVEC() (program/test/interrupt/int.hsm).
The pulse half H is never fixed: each program first runs without a pulse, and H is taken
from the half where PC reaches the program's loop (it prints the address as L<hex>).
"""
import argparse
import os
import re
import shutil
import statistics
import subprocess
import sys
import threading
import traceback
import atexit
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROG = ROOT / "program"
# INTTEST_OUT: another folder next to int/ (same depth: the variant wrappers include ../../../test/interrupt/...),
# so a second run does not clash with one in int/
OUT = PROG / "build" / "dsim" / os.environ.get("INTTEST_OUT", "int")
DSIM = ROOT / "tools" / "dsim"
SNAP = OUT / "components"
CIRCUIT = "computer-32-test.dig"
JAR = os.environ.get("DIGITAL_JAR", "D:/Users/taeh5/Digital/Digital 0.31/Digital.jar")

IRQ = 3                     # must match int.hsm
IRQ2 = 4                    # int.hsm IRQ2 (nested tests)
PULSE = 2                   # halves I-in bit IRQ stays high (the IPU latches a 1-half pulse)
ISTACK, SENT = 0x2000, 0x5A5A5A5A
MAX_HALF = 400000
STALL = 2000                # -Dstall: PC unchanged this many halves = stop (the programs end in jmp $). the
                            # longest PC gap in a normal run was 36 halves (11-19); it was 6000 before
# early stops (-nolimit turns them off). normal outputs are at most ~210 characters and print "L" once
# (2026-11-19, ~2000 runs); a restart prints "L" again (banner), a loop on one handler repeats one character
LIMITS = True
MAX_OUT, MAX_REPEAT = 1000, 100     # dots are not counted: the loops print them until an IRQ comes
NO_BANNER = {"ext", "sysprev", "sys0", "tbl", "wpsys"}     # these do not start with "L"
# boot sequencer start (S/R), then the boot ROM copies the -Drom image to RAM
START = "6:R=0;10:S=1;16:S=0"
TAP = "cpu-32.dig:PCS=PC,com1,com2,INT_req=INT_req"

PROGRAMS = ["ext", "prec", "sled", "misp", "flc", "iret", "iret2", "iretpre", "sys", "sysprev", "sysirq",
            "nest", "wpsys", "sysmem", "user", "sys0", "tbl"]
# programs that are also built as variants: image int_<name>_<tag> from a wrapper in OUT
# that #defines the given line and includes the program (dasm has no -D)
VARIANTS = {"sysprev": {f"p{n}": f"NPAD {n}" for n in range(24)},
            "iretpre": {f"c{k}": f"C{k} 1" for k in range(8)}}
SYSPREV = ["st", "set", "addi", "nop", "set+nop", "mispredicted loop exit", "jmp.one",
           "predicted not-taken jmp.zero", "ld", "cmpi", "loop exit+nop", "loop exit+set"]
IRETPRE = ["st", "nop", "set", "4 nops", "ld", "ld ld (int_nest)", "ld ld", "st st .. ld ld"]

# item -> why it fails today. Remove the entry when the item passes.
XFAIL = {
}


def tool(*cmd):
    p = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if p.returncode:
        raise SystemExit(f"{cmd[0]} failed:\n{p.stdout}{p.stderr}")


def build(name):
    obj = OUT / f"int_{name}.o"
    image = OUT / f"int_{name}.bin"
    tool(ROOT / "bin" / "dasm.exe", PROG / "test" / "interrupt" / f"int_{name}.asm", "-o", obj)
    tool(ROOT / "bin" / "dlnk.exe", obj, "-o", image)
    for tag, define in VARIANTS.get(name, {}).items():
        v = f"int_{name}_{tag}"
        (OUT / f"{v}.asm").write_text(f"#define {define}\n#include<../../../test/interrupt/int_{name}.asm>\n")
        tool(ROOT / "bin" / "dasm.exe", OUT / f"{v}.asm", "-o", OUT / f"{v}.o")
        tool(ROOT / "bin" / "dlnk.exe", OUT / f"{v}.o", "-o", OUT / f"{v}.bin")


def words(name):
    """{address: word} of the text in a built image (after the 8 byte header)"""
    data = (OUT / f"int_{name}.bin").read_bytes()[8:]
    return {i: int.from_bytes(data[i:i + 4], "little") for i in range(0, len(data) - 3, 4)}


def op_addrs(name, op):
    return [a for a, w in sorted(words(name).items()) if w >> 24 == op]


def glog(text):
    """the handler log printed by DUMPLOG, or None"""
    m = re.search(r"G((?:[0-9A-F]{8} )*)g", text)
    return [hx(x) for x in m.group(1).split()] if m else None


def first_hit(d, addr, after=0):
    for h, v in d["PC"]:
        if h >= after and v == format(addr, "x"):
            return h
    return None


class Run:
    def __init__(self, name, seed, h, text, tail, delta, tag=""):
        self.name, self.seed, self.h, self.text, self.tail, self.delta, self.tag = name, seed, h, text, tail, delta, tag

    @property
    def pc(self):
        m = re.search(r"PC=([0-9a-fA-Z]+)", self.tail)
        return m.group(1) if m else "?"

    def where(self):
        return f"z{self.seed}/H={self.h}/PC={self.pc}"


def at_string(pulses):
    """-Dat for the start sequence and PULSE-half pulses [(half, I-in bit), ...]"""
    edges = sorted({t for h, _ in pulses for t in (h, h + PULSE)})
    parts = [START]
    for t in edges:
        v = 0
        for h, b in pulses:
            if h <= t < h + PULSE:
                v |= 1 << b
        parts.append(f"{t}:I-in={v}")
    return ";".join(parts)


def sim(name, seed=1, h=None, cols=None, tag=None, pulses=None):
    """One run. h = half of the IRQ rising edge (None: no pulse). pulses = [(half, bit), ...]
    for more than one pulse (h is then only used in the tag)."""
    if pulses is None:
        pulses = [] if h is None else [(h, IRQ)]
    tag = tag or f"{name}-z{seed}-" + ("none" if not pulses else "-".join(f"{p}b{b}" for p, b in pulses))
    at = at_string(pulses)
    opts = ["-Ddip=", f"-Dzseed={seed}", f"-Drom={OUT / f'int_{name}.bin'}", f"-Dstall={STALL}", "-Didle=20000",
            "-Dset=S=0,R=1,B_used=0,I-in=0", f"-Dat={at}", f"-Dtap={TAP}"]
    if LIMITS:      # a restart loop or a handler entered over and over: stop early instead of at MAX_HALF
        opts += [f"-DmaxOut={MAX_OUT}", f"-DmaxRepeat={MAX_REPEAT}", "-DlimitSkip=."]
        if name.split("_")[0] not in NO_BANNER:
            opts += ["-Dbanner=L"]
    delta = None
    if cols:
        delta = OUT / f"{tag}.delta"
        opts += [f"-Dcols={','.join(cols)}", f"-Ddelta={delta}"]
    args = [str(SNAP / CIRCUIT), "-", "", str(MAX_HALF)]
    simfile = OUT / f"{tag}.sim"
    if ONESHOT:
        p = subprocess.run(["java", *opts, "-cp", CP, "Sim", *args], capture_output=True, text=True,
                           timeout=900, cwd=DSIM)
        stdout, err = p.stdout, p.stderr[-2000:]
        simfile.write_text(stdout)
    else:
        err = worker().run(simfile, opts, args)
        stdout = simfile.read_text() if simfile.exists() else ""
    if "[half=" not in stdout:
        raise RuntimeError(f"Sim failed for {tag}:\n{err}")
    text, tail = stdout.rsplit("[half=", 1)
    m = re.search(r"stop: ERROR (.*)\]", tail)
    if m:           # a Digital error (short circuit, ...) ended the run: always a failure (Report.show)
        DIGITAL_ERRORS.append((tag, m.group(1).strip()))
    text = re.sub(r"\[RESTART at half \d+\]", "", text.replace("\n", ""))   # Sim's note, not program output
    return Run(name, seed, h, text, tail, delta, tag)


CP = f"{JAR};{DSIM / 'build'}"
DIGITAL_ERRORS = []         # (tag, message) of the runs that stopped with "stop: ERROR" (list.append is thread safe)
ONESHOT = False             # -oneshot : one java process per run (the old way)
_local = threading.local()
_workers = []


class Worker:
    """A resident 'Sim -server' JVM: the circuit is loaded once, every run builds a new model."""

    def __init__(self):
        self.log = open(OUT / f"worker-{len(_workers)}.log", "w")
        self.p = subprocess.Popen(["java", "-cp", CP, "Sim", "-server"], cwd=DSIM, text=True,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log)

    def run(self, simfile, opts, args):
        """'' when the run finished, else the error"""
        fields = ["RUN", str(simfile)] + [o[2:] for o in opts] + ["--"] + args
        assert not any("\t" in f or "\n" in f for f in fields)
        if simfile.exists():
            simfile.unlink()
        self.p.stdin.write("\t".join(fields) + "\n")
        self.p.stdin.flush()
        ans = self.p.stdout.readline().strip()
        if not ans:
            return f"worker exited ({self.p.poll()}), see {self.log.name}"
        return "" if ans == "OK" else ans

    def close(self):
        try:
            self.p.stdin.close()
            self.p.wait(30)
        except Exception:
            self.p.kill()
        self.log.close()


def worker():
    w = getattr(_local, "w", None)
    if w is None or w.p.poll() is not None:
        w = _local.w = Worker()
        _workers.append(w)
    return w


@atexit.register
def _close_workers():
    for w in _workers:
        w.close()


def read_delta(path):
    """{signal: [(half, value), ...]}, values are the strings Sim wrote (hex, Z)."""
    with open(path) as f:
        names = f.readline().split()
        out = {n: [] for n in names}
        for line in f:
            p = line.split()
            if len(p) == 3:
                out[names[int(p[1])]].append((int(p[0]), p[2]))
    return out


def value_at(changes, h, default="0"):
    """Value of a delta signal at half h (the last change at or before h)."""
    v = default
    for t, x in changes:
        if t > h:
            break
        v = x
    return v if v != "Z" else "0"


def high_spans(changes, end=10 ** 9):
    spans, start = [], None
    for h, v in changes:
        on = v not in ("0", "Z")
        if on and start is None:
            start = h
        elif not on and start is not None:
            spans.append((start, h))
            start = None
    if start is not None:
        spans.append((start, end))
    return spans


FULL = False                # -full: every even half for every seed instead of sweep()
SPARSE = 6                  # step outside the dense windows; 6 is not a divisor of the 16-half loop periods,
                            # so the samples still hit every even phase


def sweep(lo, hi, dense, first, w=24, step=None):
    """Pulse halves in [lo, hi). Every even half within w of an event in dense; elsewhere every step
    (default SPARSE) halves, and only for the first seed (first=True). With FULL, every even half.
    11-19 baseline (1500 sweep runs, seeds 1-3): no half gave a different output for another seed."""
    lo &= ~1
    if FULL:
        return list(range(lo, hi, 2))
    out = set()
    for e in dense:
        out.update(h for h in range((e - w) & ~1, e + w + 1, 2) if lo <= h < hi)
    if first:
        out.update(range(lo, hi, step or SPARSE))
    return sorted(out)


def calibrate(name, seed):
    """Run without a pulse. Returns (run, loop address, halves where PC reached it, period, delta)."""
    r = sim(name, seed, cols=["PC", "INT_req"], tag=f"{name}-z{seed}-cal")
    m = re.match(r"L([0-9A-F]{8}) ", r.text)
    if not m:
        raise RuntimeError(f"{name} z{seed}: no L<addr> in {r.text[:80]!r} ({r.tail.strip()})")
    addr = int(m.group(1), 16)
    d = read_delta(r.delta)
    hits = [h for h, v in d["PC"] if v == format(addr, "x")]
    if not hits:
        raise RuntimeError(f"{name} z{seed}: PC never reached {addr:#x}")
    gaps = [b - a for a, b in zip(hits, hits[1:])]
    return r, addr, hits, int(statistics.median(gaps)) if gaps else 0, d


HEX = r"([0-9A-F]{8}) "
ENTRY = re.compile(r"L" + HEX + r"S(\.*)I" + HEX * 5 + "i")          # prec, sled
ENTRY_M = re.compile(r"L" + HEX + r"S(\.*)I" + HEX * 7 + "i")        # misp


def hx(s):
    return int(s, 16)


def pushed(m4, m0):
    """The return PC wherever it went, for the checks that do not test the address."""
    return m4 if m4 != SENT else m0


class Report:
    def __init__(self):
        self.rows = []

    def item(self, key, text, ok, detail=""):
        """ok: True, False, None = could not be judged in this run"""
        if ok is None:
            st = "SKIP"
        elif key in XFAIL:
            st = "XPASS" if ok else "XFAIL"
        else:
            st = "PASS" if ok else "FAIL"
        self.rows.append((key, st, text, detail))

    def show(self):
        for tag, msg in sorted(set(DIGITAL_ERRORS)):
            self.rows.append(("0", "ERROR", f"Digital error in run {tag}", f"{msg} (details: {tag}.sim, worker log)"))
        DIGITAL_ERRORS.clear()
        print()
        order = lambda row: (int(re.match(r"\d+", row[0]).group()), row[0])
        for key, st, text, detail in sorted(self.rows, key=order):
            print(f"{st:5} {key:4} {text}" + (f"\n           {detail}" if detail else ""))
        n = {}
        for r in self.rows:
            n[r[1]] = n.get(r[1], 0) + 1
        print("\n" + " ".join(f"{k}={v}" for k, v in sorted(n.items())))
        return not any(r[1] in ("FAIL", "XPASS", "ERROR") for r in self.rows)


def brief(items, limit=6):
    items = list(items)
    return ", ".join(items[:limit]) + (f" ... ({len(items)})" if len(items) > limit else "")


def t_ext(rep, pool, a):
    r = sim("ext", a.seeds[0])
    m = re.match(HEX * 7 + "E", r.text)
    got = [hx(x) for x in m.groups()] if m else None
    detail = f"output {r.text!r}"
    rep.item("1a", "stexp/ldexp: istack, intlevel and inttmplv (lev b0-3, mask b4, ker b7), cpuid stable",
             bool(m) and got[2:5] == [0x12345670, 0x98, 0x87] and got[0] == got[1], detail)
    rep.item("1b", "stexp/ldexp: pbase plimit", bool(m) and got[5:] == [0x12340000, 0x12345000], detail)


def t_prec(rep, pool, a):
    cals = list(pool.map(lambda z: calibrate("prec", z), a.seeds))
    bad6 = [f"z{c[0].seed}" for c in cals if any(v != "0" for _, v in c[4]["INT_req"])]
    rep.item("6a", "INT_req stays 0 without syscall (int_prec, no pulse)", not bad6, brief(bad6))
    jobs = []
    for r, addr, hits, period, d in cals:
        base = hits[len(hits) // 2] & ~1
        print(f"prec z{r.seed}: loop={addr:#x} period={period} halves, H from {base}", flush=True)
        jobs += [(r.seed, base + 2 * i) for i in range(period)]     # two periods
    runs = list(pool.map(lambda j: sim("prec", j[0], j[1]), jobs))

    done, stalled = [], []
    for r in runs:
        m = ENTRY.match(r.text)
        if m:
            done.append((r, {"loop": hx(m.group(1)), "dots": len(m.group(2)), "g5": hx(m.group(3)),
                             "ist": hx(m.group(4)), "m4": hx(m.group(5)), "m0": hx(m.group(6)), "lv": hx(m.group(7))}))
        else:
            stalled.append(r.where())
    rep.item("2", f"entry at every loop phase runs the handler to the end ({len(runs)} runs)", not stalled,
             f"{len(stalled)} stalled: " + brief(stalled, 8) if stalled else "")

    bad = [f"{r.where()} dots={f['dots']} gen5={f['g5']}" for r, f in done if f["dots"] not in (f["g5"], f["g5"] - 1)]
    rep.item("3a", "precise: dots and gen[5] agree in the handler", (not bad) if done else None, brief(bad))

    def consistent(f, shift=0):
        off = pushed(f["m4"], f["m0"]) - shift - f["loop"]
        want = {0: f["g5"], 4: f["g5"] - 1, 8: f["g5"] - 1, 12: f["g5"], 16: f["g5"]}.get(off)
        return want is not None and want == f["dots"]
    bad = [f"{r.where()} R={pushed(f['m4'], f['m0']):#x} dots={f['dots']} gen5={f['g5']}" for r, f in done if not consistent(f)]
    rep.item("3b", "precise: the return PC is the commit point of dots and gen[5]", (not bad) if done else None,
             f"{len(bad)}/{len(done)} wrong: " + brief(bad, 3) if bad else "")
    bad = [f"{r.where()} R={pushed(f['m4'], f['m0']):#x} dots={f['dots']} gen5={f['g5']}" for r, f in done
           if not (consistent(f) or consistent(f, 4))]
    rep.item("3c", "same, also accepting return PC + 4 (shows real errors while item 5 is open)",
             (not bad) if done else None, brief(bad, 3))
    bad = [r.where() for r, f in done if not (f["ist"] == ISTACK - 4 and f["m4"] != SENT and f["m0"] == SENT)]
    rep.item("4", "push: istack -= 4, mem[istack] = return PC, mem[old istack] kept", (not bad) if done else None,
             f"{len(bad)}/{len(done)} wrong: " + brief(bad) if bad else f"{len(done)} runs")
    bad = [f"{r.where()} level={f['lv']:#x}" for r, f in done if f["lv"] != 0x91]
    rep.item("13", "int-level in the handler = {ker 1, mask 1, level 1} = 0x91", (not bad) if done else None,
             f"{len(bad)}/{len(done)} wrong: " + brief(bad, 2) if bad else f"{len(done)} runs")


def t_sled(rep, pool, a):
    r, addr, hits, _, d = calibrate("sled", a.seeds[0])
    base = (hits[0] + 80) & ~1
    print(f"sled: start={addr:#x}, H from {base}", flush=True)
    runs = list(pool.map(lambda h: sim("sled", a.seeds[0], h), [base + 2 * i for i in range(16)]))
    done, stalled, off4, other = [], [], 0, []
    for r in runs:
        m = ENTRY.match(r.text)
        if not m:
            stalled.append(r.where())
            continue
        g5, m4, m0 = hx(m.group(3)), hx(m.group(5)), hx(m.group(6))
        R = pushed(m4, m0)
        want = addr + 4 * g5
        if R == want:
            pass
        elif R == want + 4:
            off4 += 1
        else:
            other.append(f"{r.where()} R={R:#x} want={want:#x}")
        done.append(r)
    ok = (off4 == 0 and not other) if done else None
    detail = f"{len(done)} entered, {off4} are want+4" + (", other: " + brief(other, 3) if other else "")
    if stalled:
        detail += f"; stalled: {brief(stalled, 3)}"
    rep.item("5", "return PC = first uncommitted instruction (sled + 4*gen[5])", ok, detail)


def misp_states(n=4, n_iter=400):
    """(offset from loop, gen5, gen6, gen7) of every instruction boundary on the correct path.
    int_misp: 4 words between the addi of gen5 and the branch; int_flc: 6."""
    out, g5, g6, g7 = set(), 0, 0, 0
    odd = 4 * (n + 1)
    for i in range(1, n_iter + 1):
        out.add((0, g5, g6, g7))
        g5 += 1
        for off in range(4, odd, 4):
            out.add((off, g5, g6, g7))
        if g5 & 1:
            out.add((odd, g5, g6, g7))
            g6 += 1
            out.add((odd + 4, g5, g6, g7))
        else:
            out.add((odd + 8, g5, g6, g7))
            g7 += 1
        out.add((odd + 12, g5, g6, g7))
        out.add((odd + 16, g5, g6, g7))
    return out


def t_misp(rep, pool, a, name="misp", keys=("10a", "10b", "10c"), states=None, seeds=None):
    states = states or misp_states()
    runs = []
    for z in seeds or a.seeds[:1]:
        r, addr, hits, period, d = calibrate(name, z)
        # a pattern is 2 iterations and the branch path differs, so sweep two whole patterns
        span = 2 * (hits[len(hits) // 2 + 2] - hits[len(hits) // 2])
        base = hits[len(hits) // 2] & ~1
        print(f"{name} z{z}: loop={addr:#x} pattern={span // 2} halves, H from {base}", flush=True)
        runs += list(pool.map(lambda h: sim(name, z, h), range(base, base + span, 2)))
    done, stalled = [], []
    for r in runs:
        m = ENTRY_M.match(r.text)
        if m:
            f = {"g5": hx(m.group(3)), "g6": hx(m.group(4)), "g7": hx(m.group(5)), "ist": hx(m.group(6)),
                 "m4": hx(m.group(7)), "m0": hx(m.group(8)), "lv": hx(m.group(9)), "loop": hx(m.group(1))}
            done.append((r, f))
        else:
            stalled.append(r.where())

    def inside(f, shift=0):
        off = pushed(f["m4"], f["m0"]) - shift - f["loop"]
        return (off, f["g5"], f["g6"], f["g7"]) in states
    show = lambda r, f: f"{r.where()} R={pushed(f['m4'], f['m0']):#x} g5={f['g5']} g6={f['g6']} g7={f['g7']}"
    what = "mispredict" if name == "misp" else f"{name} (late branch)"
    bad = [show(r, f) for r, f in done if not (inside(f) or inside(f, 4))]
    rep.item(keys[0], f"{what}: return PC (+4 allowed) and registers are on the correct path",
             (not bad) if done else None, f"{len(bad)}/{len(done)} wrong: " + brief(bad, 3) if bad else f"{len(done)} runs")
    bad = [show(r, f) for r, f in done if not inside(f)]
    rep.item(keys[1], f"{what}: same with the exact return PC", (not bad) if done else None,
             f"{len(bad)}/{len(done)} wrong" if bad else "")
    rep.item(keys[2], f"{what}: entry at every phase reaches the end of the handler ({len(runs)} runs)", not stalled,
             f"{len(stalled)} stalled: " + brief(stalled, 8) if stalled else "")


def t_flc(rep, pool, a):
    t_misp(rep, pool, a, "flc", ("10d", "10e", "10f"), misp_states(6, 150), a.seeds)


def t_iret(rep, pool, a):
    z = a.seeds[0]
    r, addr, hits, period, d = calibrate("iret", z)
    base = hits[len(hits) // 2] & ~1
    print(f"iret: loop={addr:#x} period={period} halves, H from {base}", flush=True)
    runs = list(pool.map(lambda h: sim("iret", z, h), [base + 2 * i for i in range(period // 2)]))
    entered, resumed, bad = [r for r in runs if "I" in r.text], [], []
    final = re.compile(r"L" + HEX + r"S(\.*)Ii(\.*)E" + HEX * 3 + "e")
    for r in entered:
        m = final.match(r.text)
        if m and len(m.group(2)) + len(m.group(3)) == 60 and hx(m.group(4)) == 60:
            resumed.append((r, hx(m.group(5)), hx(m.group(6))))
        else:
            bad.append(f"{r.where()} {r.text[-12:]!r}")
    rep.item("7a", "iret: the program goes on, 60 dots and gen[5] = 60", (not bad) if entered else None,
             f"{len(entered)} entered, {len(bad)} wrong: " + brief(bad, 3) if bad else f"{len(entered)} runs")
    wrong = [r.where() for r, ist, _ in resumed if ist != ISTACK]
    rep.item("7b", "iret: istack back to ISTACK", (not wrong) if resumed else None, brief(wrong))
    wrong = [r.where() for r, _, lv in resumed if lv != 0x80]
    rep.item("7c", "iret: int-level back to 0x80 (INIT)", (not wrong) if resumed else None, brief(wrong))


def t_sys(rep, pool, a):
    z = a.seeds[0]
    r = sim("sys", z, cols=["PC", "INT_req"], tag=f"sys-z{z}-run")
    rises = [h for h, s in high_spans(read_delta(r.delta)["INT_req"])]
    rep.item("6b", "syscall raises INT_req", bool(rises), f"high at half {rises[:3]}, output {r.text!r}")
    m = re.match(r"L" + HEX + "AH" + HEX + "h", r.text)
    after = re.match(r"L" + HEX, r.text)
    want = hx(after.group(1)) if after else None
    ok = bool(m) and hx(m.group(2)) == want
    rep.item("8a", "syscall enters the handler, return PC = next word", ok,
             f"want R={want:#x}, output {r.text!r}" if want is not None else f"output {r.text!r}")
    rep.item("8b", "syscall returns, BC printed", r.text.endswith("BCE"), f"output {r.text!r}, stop [{r.tail.strip()}]")


def t_tbl(rep, pool, a):
    r = sim("tbl", a.seeds[0])
    ok = r.text == "A00000105 00000009 E"
    rep.item("9", "interst/interld: int_vec and int_pri read back", ok, f"output {r.text!r}, stop [{r.tail.strip()}]")


def lv5(v):
    """{mask, level}, the bits int-level compares"""
    return v & 0x1F


def t_sysprev(rep, pool, a):
    pads = range(len(VARIANTS["sysprev"]))
    runs = list(pool.map(lambda n: sim(f"sysprev_p{n}", a.seeds[0]), pads))
    want = [s + 4 for s in op_addrs("sysprev_p0", 0x76)]
    done, hung = [], []
    for n, r in zip(pads, runs):
        log = glog(r.text)
        (done if log is not None and r.text.endswith("e") else hung).append((n, r, log))
    rep.item("14c", f"syscall + iret run through for every boot length (NPAD {pads[0]}-{pads[-1]})", not hung,
             "stalled: " + ", ".join(f"NPAD {n} PC={r.pc}" for n, r, _ in hung) if hung else "")
    if not done:
        rep.item("14a", "syscall return PC = next word after each kind of instruction", None, "no run finished")
        return
    bad, stk = set(), []
    for n, r, log in done:
        for i, w in enumerate(want):
            got = log[i] if i < len(log) else None
            if got != w:
                bad.add(f"{i} {SYSPREV[i]}: R={'-' if got is None else hex(got)} want {w:#x}")
        m = re.search(r"g" + HEX * 3 + "e", r.text)
        ist, sp = (hx(m.group(2)), hx(m.group(3))) if m else (None, None)
        if (ist, sp) != (ISTACK, 0x3000):
            stk.append(f"NPAD {n} istack={ist if ist is None else hex(ist)} stack={sp if sp is None else hex(sp)}")
    rep.item("14a", f"syscall return PC = next word ({len(want)} cases, {len(done)} boot lengths)",
             not bad, brief(sorted(bad, key=lambda s: int(s.split()[0])), 4))
    rep.item("14d", "syscall + iret: istack and stack back at the end (ISTACK, 0x3000)", not stk, brief(stk, 3))


def t_iretpre(rep, pool, a):
    def one(k):
        name = f"iretpre_c{k}"
        r, loop, hits, _, d = calibrate(name, a.seeds[0])
        return k, sim(name, a.seeds[0], hits[len(hits) // 2] & ~1)
    hung, bad = [], []
    for k, r in pool.map(one, range(len(IRETPRE))):
        n, log = r.text.count("I"), glog(r.text)
        if log is None:
            hung.append(f"C{k} {IRETPRE[k]}: " + ("syscall" if n < 2 else "IRQ") + f" iret, PC={r.pc}")
        elif n != 2 or len(log) != 2 or not r.text.endswith("E0000003C G" + format(log[0], "08X") + " "
                                                               + format(log[1], "08X") + " g"):
            bad.append(f"C{k} {r.text[-30:]!r}")
    rep.item("22", f"iret returns whatever comes before it ({len(IRETPRE)} handler endings, entered by "
             "syscall and by IRQ)", not hung and not bad, brief(hung + bad, 4))


def banner(r, n):
    """match of 'L' + n hex words at the start of r.text; a run that stopped before (e.g. mid-banner)
    raises RuntimeError, which fails the group in main() instead of an AttributeError on None"""
    m = re.match(r"L" + HEX * n, r.text)
    if not m:
        raise RuntimeError(f"{r.tag}: no L + {n} words in {r.text[:120]!r} ({r.tail.strip()})")
    return m


def t_sysirq(rep, pool, a):
    jobs = []
    info = {}
    for z in a.seeds:
        r, sc, hits, _, d = calibrate("sysirq", z)
        m = banner(r, 3)
        main_, after = hx(m.group(2)), hx(m.group(3))
        rise = [s for s, _ in high_spans(d["INT_req"])]
        if not rise:
            raise RuntimeError(f"sysirq z{z}: no INT_req in the calibration run ({r.text!r})")
        lo, hi = (hits[0] - 40) & ~1, rise[0] + 200
        print(f"sysirq z{z}: sc={sc:#x} INT_req at {rise[0]}, H {lo}..{hi}", flush=True)
        info[z] = (sc, main_, after, r)
        jobs += [(z, h) for h in sweep(lo, hi, [lo + 24, rise[0] - 20, rise[0] + 60], z == a.seeds[0], 40)]
    runs = list(pool.map(lambda j: sim("sysirq", j[0], j[1]), jobs))
    cal = [info[z][3] for z in a.seeds]
    # 14b: syscall level, from the runs without a pulse
    lvs = {f"z{r.seed}": (glog(r.text) or [0, 0])[1] for r in cal}
    rep.item("14b", "int-level in the syscall handler = {ker 1, mask 1, level int_pri[1] = 1} = 0x91",
             all(v == 0x91 for v in lvs.values()), ", ".join(f"{k}: {v:#x}" for k, v in lvs.items()))
    stalled, lost, wrong, levels, order = [], [], [], [], {"Q first": 0, "S first": 0}
    final = re.compile(r"E" + HEX * 2 + r"G((?:[0-9A-F]{8} )*)g" + HEX * 2 + "e")
    for r in runs:
        sc, main_, after, _ = info[r.seed]
        m = final.search(r.text)
        if not m:
            stalled.append(r.where())
            continue
        g5, g6, log, ist, lv = hx(m.group(1)), hx(m.group(2)), glog(r.text), hx(m.group(4)), hx(m.group(5))
        seq = re.sub(r"[^QS]", "", r.text.split("E")[0])
        if seq not in ("QS", "SQ") or len(log) != 4:
            lost.append(f"{r.where()} entries {seq!r}")
            continue
        why = None
        rq, rs = (log[0], log[2]) if seq == "QS" else (log[2], log[0])
        if rs != after:
            why = f"{seq} syscall R={rs:#x} want {after:#x}"
        elif seq == "QS" and not main_ <= rq <= sc:
            why = f"QS IRQ R={rq:#x} not in main..sc"
        elif seq == "SQ" and rq < after:
            why = f"SQ IRQ R={rq:#x} before after (nested in the syscall handler?)"
        elif (g5, g6, ist, lv5(lv)) != (6, 60, ISTACK, 0):
            why = f"gen5={g5} gen6={g6} istack={ist:#x} level={lv:#x}"
        if log[1 if seq[0] == "Q" else 3] != 0x91:
            levels.append(r.where())
        if why:
            wrong.append(f"{r.where()} {why}")
        else:
            order[f"{seq[0]} first"] += 1
    rep.item("15a", f"IRQ around syscall: the program finishes ({len(runs)} runs)",
             not stalled, f"{len(stalled)} stalled: " + brief(stalled, 6) if stalled else "")
    rep.item("15b", "IRQ around syscall: both handlers run once (the IRQ is not lost)", not lost,
             f"{len(lost)} wrong: " + brief(lost, 4) if lost else "")
    rep.item("15c", "IRQ around syscall: IRQ before the syscall commits (R <= sc) or after its iret, "
             "syscall R = next word", (not wrong) if len(lost) < len(runs) else None,
             f"{len(wrong)} wrong: " + brief(wrong, 3) if wrong else ", ".join(f"{k}: {v}" for k, v in order.items()))
    rep.item("15d", "IRQ around syscall: int-level in the IRQ handler = 0x91", not levels, brief(levels))


def t_nest(rep, pool, a):
    jobs, info = [], {}
    for z in a.seeds:
        r, loop, hits, _, d = calibrate("nest", z)
        m = banner(r, 4)
        open_, close, main_ = hx(m.group(2)), hx(m.group(3)), hx(m.group(4))
        h1 = hits[len(hits) // 2] & ~1
        lo = sim("nest", z, cols=["PC"], pulses=[(h1, IRQ)], tag=f"nest-z{z}-lo")
        dl = read_delta(lo.delta)
        entry, hc = first_hit(dl, 8, h1), None
        if entry is not None:
            hc = first_hit(dl, close, entry)
            ho = first_hit(dl, open_, entry) or entry
        if entry is None or hc is None:
            raise RuntimeError(f"nest z{z}: the low IRQ alone did not reach close ({lo.text!r})")
        info[z] = (loop, open_, close, main_)
        print(f"nest z{z}: low at {h1}, handler {entry}..close {hc}", flush=True)
        jobs += [(z, h1, h) for h in sweep(entry, hc + 40, [entry, ho, hc, hc + 16], z == a.seeds[0])]
    runs = list(pool.map(lambda j: sim("nest", j[0], pulses=[(j[1], IRQ), (j[2], IRQ2)],
                                       tag=f"nest-z{j[0]}-{j[1]}-{j[2]}"), jobs))
    final = re.compile(r"E" + HEX * 3 + r"G")
    stalled, wrong, kinds = [], [], {"nested": 0, "after": 0}
    for r in runs:
        loop, open_, close, main_ = info[r.seed]
        where = f"z{r.seed}/H2={r.tag.split('-')[-1]}"
        m, log = final.search(r.text), glog(r.text)
        # only the handler marks between "S" and "E": the L addresses and the log dump
        # are hex and can hold A, B or E ("S" is not a hex digit)
        body = r.text[r.text.find("S"):] if "S" in r.text else ""
        seq = re.sub(r"[^ABbz]", "", body.split("E")[0])
        if not m or log is None:
            stalled.append(f"{where} {seq!r}")
            continue
        g5, ist, lv = hx(m.group(1)), hx(m.group(2)), hx(m.group(3))
        dots = r.text.count(".")
        why = None
        if (g5, dots, ist, lv5(lv)) != (60, 60, ISTACK, 0):
            why = f"gen5={g5} dots={dots} istack={ist:#x} level={lv:#x}"
        elif seq == "ABbz" and len(log) == 6:
            rhi, lvhi, tmphi, tmplo = log[2], log[3], log[4], log[5]
            if not open_ <= rhi <= close:
                why = f"nested R={rhi:#x} not in {open_:#x}..{close:#x}"
            elif (lv5(lvhi), lv5(tmphi), lv5(tmplo)) != (0x12, 0x01, 0):
                why = f"nested hi level={lvhi:#x} tmplv={tmphi:#x}, lo tmplv after restore={tmplo:#x}"
            else:
                kinds["nested"] += 1
        elif seq == "AzBb" and len(log) == 6:
            tmplo, rhi, lvhi, tmphi = log[2], log[3], log[4], log[5]
            if rhi < main_:
                why = f"after R={rhi:#x} in the low handler"
            elif (lv5(lvhi), lv5(tmphi), lv5(tmplo)) != (0x12, 0, 0):
                why = f"after hi level={lvhi:#x} tmplv={tmphi:#x}, lo tmplv={tmplo:#x}"
            else:
                kinds["after"] += 1
        else:
            why = f"entries {seq!r} log {len(log)}"
        if why:
            wrong.append(f"{where} {why}")
    rep.item("16a", f"nested IRQ: both handlers run and main finishes ({len(runs)} runs)", not stalled,
             f"{len(stalled)} stalled: " + brief(stalled, 4) if stalled else "")
    rep.item("16b", "nested IRQ: high enters only while the low handler has mask 0 (else after its iret), "
             "levels 0x12 / tmplv 0x01, int-tmplv restored, istack and int-level back",
             not wrong if runs else None,
             f"{len(wrong)} wrong: " + brief(wrong, 3) if wrong else ", ".join(f"{k}: {v}" for k, v in kinds.items()))


def t_iret2(rep, pool, a):
    jobs, info = [], {}
    iret = op_addrs("iret2", 0x77)[0]
    for z in a.seeds:
        r, loop, hits, _, d = calibrate("iret2", z)
        main_ = hx(banner(r, 2).group(2))
        h1 = hits[len(hits) // 2] & ~1
        one = sim("iret2", z, cols=["PC"], pulses=[(h1, IRQ)], tag=f"iret2-z{z}-one")
        dl = read_delta(one.delta)
        entry = first_hit(dl, 8, h1)
        hi = first_hit(dl, iret, entry or h1)
        if entry is None or hi is None:
            raise RuntimeError(f"iret2 z{z}: one IRQ did not reach iret ({one.text!r})")
        info[z] = main_
        print(f"iret2 z{z}: first IRQ at {h1}, entry {entry}, iret fetched {hi}", flush=True)
        jobs += [(z, h1, h) for h in sweep(entry, hi + 100, [hi], z == a.seeds[0], 24, 2)]
    runs = list(pool.map(lambda j: sim("iret2", j[0], pulses=[(j[1], IRQ), (j[2], IRQ)], cols=["I-act", "I-acp"],
                                       tag=f"iret2-z{j[0]}-{j[1]}-{j[2]}"), jobs))
    final = re.compile(r"E" + HEX * 3 + r"G")
    stalled, wrong, ignored = [], [], 0
    for r in runs:
        m, log = final.search(r.text), glog(r.text)
        h2 = int(r.tag.split('-')[-1])
        where = f"z{r.seed}/H2={h2}"
        if not m or log is None:
            stalled.append(f"{where} PC={r.pc}")
            continue
        # the test device latch (RS-64 in computer-32-test) ignores a pulse whose sampling edge (half H2)
        # still sees INT-act or INT-acp for that line. at the acp edge itself S = R = 1 and the RS_FF
        # result follows the seed, so a busy pulse may give 1 or 2 entries
        d = read_delta(r.delta)
        busy = any(int(value_at(d[c], h2 - 1), 16) >> IRQ & 1 for c in ("I-act", "I-acp"))
        want = (1, 2) if busy else (2,)
        ignored += busy
        g5, ist, lv, n = hx(m.group(1)), hx(m.group(2)), hx(m.group(3)), r.text.count("I")
        if n not in want or len(log) != n:
            wrong.append(f"{where} {n} entries (want {'/'.join(map(str, want))})")
        elif (g5, r.text.count("."), ist, lv5(lv)) != (60, 60, ISTACK, 0):
            wrong.append(f"{where} gen5={g5} istack={ist:#x} level={lv:#x}")
        elif min(log) < info[r.seed]:
            wrong.append(f"{where} R={log[0]:#x},{log[1]:#x} (one is in the handler)")
    rep.item("17a", f"2nd IRQ around iret: the program finishes ({len(runs)} runs)", not stalled,
             f"{len(stalled)} stalled: " + brief(stalled, 4) if stalled else "")
    rep.item("17b", "2nd IRQ around iret: 2 entries (1 or 2 if the pulse comes while I-act|I-acp), both return PCs "
             "in main, istack and int-level back, gen5 = 60",
             not wrong if runs else None,
             (f"{len(wrong)} wrong: " + brief(wrong, 3) if wrong else "") + f" ({ignored} pulses during act|acp)")


def t_wpsys(rep, pool, a):
    sc = {format(x, "x") for x in op_addrs("wpsys", 0x76)}
    runs = list(pool.map(lambda z: sim("wpsys", z, cols=["PC", "INT_req"], tag=f"wpsys-z{z}"), a.seeds))
    bad, fetched = [], 0
    for r in runs:
        d = read_delta(r.delta)
        fetched += sum(1 for _, v in d["PC"] if v in sc)
        req = high_spans(d["INT_req"])
        if req or "S" in r.text or not r.text.endswith("E00000004 Gg"):
            bad.append(f"z{r.seed} INT_req={req[:2]} output {r.text[-24:]!r}")
    rep.item("18", "syscall on the wrong path: no INT_req, no entry, the program finishes",
             (not bad) if fetched else None,
             brief(bad, 3) if bad else f"wrong-path syscall fetched {fetched} times in {len(runs)} runs")


def t_sysmem(rep, pool, a):
    runs = list(pool.map(lambda z: sim("sysmem", z, tag=f"sysmem-z{z}"), a.seeds))
    r_bad, m_bad, l_bad = [], [], []
    for r in runs:
        m = re.match(r"L" + HEX * 2 + r"E" + HEX, r.text)
        log = glog(r.text)
        if not m or log is None or len(log) != 8:
            r_bad.append(f"z{r.seed} stalled PC={r.pc} output {r.text[-20:]!r}")
            continue
        a1, a2, g8 = hx(m.group(1)), hx(m.group(2)), hx(m.group(3))
        if (log[0], log[4]) != (a1, a2):
            r_bad.append(f"z{r.seed} R={log[0]:#x},{log[4]:#x} want {a1:#x},{a2:#x}")
        want = [0x11111111, SENT, 0, 0x11111111, 0x33333333, 0x22222222]
        got = log[1:4] + log[5:8]
        if got != want:
            m_bad.append(f"z{r.seed} X,Z,gen6 = " + " ".join(f"{x:#x}" for x in got))
        if g8 != a1:
            l_bad.append(f"z{r.seed} gen8={g8:#x} want {a1:#x}")
    rep.item("19a", "syscall after st / ld: return PC = next word", not r_bad, brief(r_bad, 3))
    full = len(runs) - sum("stalled" in x for x in r_bad)
    rep.item("19b", "syscall after st / ld: older st and ld done, younger st not done in the handler",
             (not m_bad) if full else None, brief(m_bad, 3))
    rep.item("19c", "load after syscall sees the handler's store", (not l_bad) if full else None, brief(l_bad, 3))


def t_user(rep, pool, a):
    runs = list(pool.map(lambda z: sim("user", z, tag=f"user-z{z}"), a.seeds))
    ok_a, ok_b, det = [], [], []
    for r in runs:
        m = re.match(r"L" + HEX * 2, r.text)
        log = glog(r.text) or []
        u1, u2 = (hx(m.group(1)), hx(m.group(2))) if m else (None, None)
        det.append(f"z{r.seed} {r.text[r.text.find('K'):r.text.find('E')]!r} log " + " ".join(f"{x:x}" for x in log))
        # user code makes no memory access (the MMU keeps user mode off the terminal): it puts a mark
        # in gen[7] and calls syscall, sysh prints "S<mark>" and logs {R, int-level, int-tmplv, istack}.
        # the 2nd syscall ('w') proves the iret went back to user
        ok_a.append(len(log) == 8 and log[0] == u1 and log[4] == u2 and "KSUSwE" in r.text)
        ok_b.append(len(log) == 8 and all(log[k] & 0x80 == 0x80 and log[k + 1] & 0x80 == 0 for k in (1, 5)))
    rep.item("20a", "user mode: syscall from user enters (R = next word) and returns to user", all(ok_a), det[0])
    rep.item("20b", "user mode: in the handler int-level ker = 1, int-tmplv ker = 0", all(ok_b), det[0])
    rep.item("20c", "user mode: stexp traps (assumed id 0), R = the stexp, istack unchanged", None,
             "not run: the privilege fault does not store its id yet and re-runs the stexp forever")


def t_sys0(rep, pool, a):
    r = sim("sys0", a.seeds[0])
    rep.item("21a", "syscall with int_pri[1] = 0: the handler does not run", "S" not in r.text,
             f"output {r.text!r}")
    sc = op_addrs("sys0", 0x76)[0]
    log = glog(r.text)
    ok = (r.text.count("P") == 1 and "BE" in r.text and log is not None and len(log) == 2
          and log[0] == sc + 4 and log[1] == 0x9f)
    rep.item("21b", "syscall with int_pri[1] = 0: enters id 0 once, R = next word, int-level 0x9f, "
             "iret returns and the program finishes", ok,
             f"want R={sc + 4:#x}, output {r.text[:60]!r}, stop [{r.tail.strip()}]")


TESTS = {"ext": t_ext, "prec": t_prec, "sled": t_sled, "misp": t_misp, "flc": t_flc, "iret": t_iret,
         "iret2": t_iret2, "iretpre": t_iretpre, "sys": t_sys, "sysprev": t_sysprev, "sysirq": t_sysirq, "nest": t_nest,
         "wpsys": t_wpsys, "sysmem": t_sysmem, "user": t_user, "sys0": t_sys0, "tbl": t_tbl}


def lock_out():
    """Two runs in OUT clash (shared .sim files, -snap copy), so refuse to start while another one holds it."""
    lock = OUT / "inttest.lock"
    try:
        fd = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError:
        pid = lock.read_text().strip() or "?"
        alive = pid.isdigit() and subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"], capture_output=True,
                                                 text=True).stdout.count(pid) > 0
        if alive:
            raise SystemExit(f"another inttest.py (pid {pid}) is using {OUT}; wait for it to end")
        lock.unlink()           # left by a run that was killed
        fd = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    os.write(fd, str(os.getpid()).encode())
    os.close(fd)
    atexit.register(lambda: lock.unlink(missing_ok=True))


def main():
    global ONESHOT, LIMITS, STALL, FULL
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-j", type=int, default=9)
    ap.add_argument("-seeds", default="1,2,3", help="zseed list for the prec sweep (others use the first)")
    ap.add_argument("-nobuild", action="store_true")
    ap.add_argument("-snap", action="store_true",
                    help="reuse the circuit copy in program/build/dsim/int/components instead of copying components/ again")
    ap.add_argument("-nolimit", action="store_true", help="no early stop on restart / long or repeated output")
    ap.add_argument("-full", action="store_true",
                    help="sysirq/nest/iret2: every even half for every seed (default: dense only near the events)")
    ap.add_argument("-stall", type=int, default=STALL, help=f"halves without progress that end a run (default {STALL})")
    ap.add_argument("-oneshot", action="store_true",
                    help="start java for every run instead of keeping one resident Sim -server per thread")
    ap.add_argument("groups", nargs="*", choices=[[], *PROGRAMS], default=[])
    a = ap.parse_args()
    ONESHOT, LIMITS, STALL, FULL = a.oneshot, not a.nolimit, a.stall, a.full
    a.seeds = [int(s) for s in a.seeds.split(",")]
    groups = a.groups or PROGRAMS
    OUT.mkdir(parents=True, exist_ok=True)
    lock_out()
    if not a.nobuild:
        for g in groups:
            build(g)
    # run on a copy: saving a circuit in Digital during a sweep must not mix two circuits
    src = ROOT / "components"
    if a.snap:
        if not (SNAP / CIRCUIT).exists():
            raise SystemExit(f"-snap: no copy in {SNAP}")
        print(f"circuit: snapshot in {SNAP.relative_to(ROOT)}, copied "
              f"{datetime.fromtimestamp((SNAP / CIRCUIT).stat().st_mtime):%Y-%m-%d %H:%M:%S}")
    else:
        print(f"circuit: {CIRCUIT} saved {datetime.fromtimestamp((src / CIRCUIT).stat().st_mtime):%Y-%m-%d %H:%M:%S}")
        shutil.rmtree(SNAP, ignore_errors=True)
        shutil.copytree(src, SNAP, ignore=lambda d, names: [n for n in names if Path(d, n).is_file() and not n.endswith(".dig")])
    rep = Report()
    with ThreadPoolExecutor(a.j) as pool:
        for g in groups:
            try:
                TESTS[g](rep, pool, a)
            except Exception as e:  # unexpected output in a calibration run etc.: fail this group, run the others
                msg = str(e) if isinstance(e, RuntimeError) else f"{type(e).__name__}: {e}\n{traceback.format_exc()}"
                print(f"{g}: stopped: {msg}", flush=True)
                rep.item("0", f"group {g} stopped, its other items were not run", False, msg)
    sys.exit(0 if rep.show() else 1)


if __name__ == "__main__":
    main()
