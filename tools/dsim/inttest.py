#!/usr/bin/env python
"""Interrupt regression tests on components/computer-32-test.dig (CPU + IPU).

  python tools/dsim/inttest.py [-j N] [-seeds 1,2,3] [-nobuild] [group...]
  group : ext prec sled misp iret sys tbl (default all). Output: program/build/dsim/int/

The emulator has no interrupts, so each item is compared with values from ISA.md 2.2, not
with the emulator. An item in XFAIL is expected to fail today: it prints XFAIL while it
fails and XPASS once it passes. The exit code is 1 on FAIL or XPASS (then remove the entry).

An interrupt is a 2-half pulse on IPU line IRQ (top-level In "I-in"). The programs set
int_vec[IRQ] = handler and int_pri[IRQ] = 1 with INTVEC() (program/test/int.hsm).
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
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROG = ROOT / "program"
OUT = PROG / "build" / "dsim" / "int"
DSIM = ROOT / "tools" / "dsim"
SNAP = OUT / "components"
CIRCUIT = "computer-32-test.dig"
JAR = os.environ.get("DIGITAL_JAR", "D:/Users/taeh5/Digital/Digital 0.31/Digital.jar")

IRQ = 3                     # must match int.hsm
PULSE = 2                   # halves I-in bit IRQ stays high (the IPU latches a 1-half pulse)
ISTACK, SENT = 0x2000, 0x5A5A5A5A
MAX_HALF = 400000
# boot sequencer start (S/R), then the boot ROM copies the -Drom image to RAM
START = "6:R=0;10:S=1;16:S=0"
TAP = "cpu-32.dig:PCS=PC,com1,com2,INT_req=INT_req,Fl_INT=Fl_INT,Fl_JPU=Fl_JPU"

PROGRAMS = ["ext", "prec", "sled", "misp", "iret", "sys", "tbl"]

# item -> why it fails today. Remove the entry when the item passes.
XFAIL = {
    "8a": "the IPU does not answer INT_req (syscall); INT_rqac is Ground",
    "8b": "same as 8a",
}


def tool(*cmd):
    p = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if p.returncode:
        raise SystemExit(f"{cmd[0]} failed:\n{p.stdout}{p.stderr}")


def build(name):
    obj = OUT / f"int_{name}.o"
    image = OUT / f"int_{name}.bin"
    tool(ROOT / "bin" / "dasm.exe", PROG / "test" / f"int_{name}.asm", "-o", obj)
    tool(ROOT / "bin" / "dlnk.exe", obj, "-o", image)


class Run:
    def __init__(self, name, seed, h, text, tail, delta):
        self.name, self.seed, self.h, self.text, self.tail, self.delta = name, seed, h, text, tail, delta

    @property
    def pc(self):
        m = re.search(r"PC=([0-9a-fA-Z]+)", self.tail)
        return m.group(1) if m else "?"

    def where(self):
        return f"z{self.seed}/H={self.h}/PC={self.pc}"


def sim(name, seed=1, h=None, cols=None, tag=None):
    """One run. h = half of the IRQ rising edge (None: no pulse)."""
    tag = tag or f"{name}-z{seed}-{'none' if h is None else h}"
    at = START if h is None else f"{START};{h}:I-in={1 << IRQ};{h + PULSE}:I-in=0"
    opts = ["-Ddip=", f"-Dzseed={seed}", f"-Drom={OUT / f'int_{name}.bin'}", "-Dstall=6000", "-Didle=20000",
            "-Dset=S=0,R=1,B_used=0,I-in=0", f"-Dat={at}", f"-Dtap={TAP}"]
    delta = None
    if cols:
        delta = OUT / f"{tag}.delta"
        opts += [f"-Dcols={','.join(cols)}", f"-Ddelta={delta}"]
    cmd = ["java", *opts, "-cp", f"{JAR};{DSIM / 'build'}", "Sim", str(SNAP / CIRCUIT), "-", "", str(MAX_HALF)]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=900, cwd=DSIM)
    if "[half=" not in p.stdout:
        raise RuntimeError(f"Sim failed for {tag}:\n{p.stderr[-2000:]}")
    (OUT / f"{tag}.sim").write_text(p.stdout)
    text, tail = p.stdout.rsplit("[half=", 1)
    return Run(name, seed, h, text.replace("\n", ""), tail, delta)


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


def overlap(a, b):
    return [(max(x[0], y[0]), min(x[1], y[1])) for x in a for y in b if max(x[0], y[0]) < min(x[1], y[1])]


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
        print()
        order = lambda row: (int(re.match(r"\d+", row[0]).group()), row[0])
        for key, st, text, detail in sorted(self.rows, key=order):
            print(f"{st:5} {key:4} {text}" + (f"\n           {detail}" if detail else ""))
        n = {}
        for r in self.rows:
            n[r[1]] = n.get(r[1], 0) + 1
        print("\n" + " ".join(f"{k}={v}" for k, v in sorted(n.items())))
        return not any(r[1] in ("FAIL", "XPASS") for r in self.rows)


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
    bad = [f"{r.where()} level={f['lv']:#x}" for r, f in done if f["lv"] != 0x11]
    rep.item("13", "int-level in the handler = {mask 1, level 1} = 0x11", (not bad) if done else None,
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


def misp_states():
    """(offset from loop, gen5, gen6, gen7) of every instruction boundary on the correct path."""
    out, g5, g6, g7 = set(), 0, 0, 0
    for i in range(1, 401):
        out.add((0, g5, g6, g7))
        g5 += 1
        for off in (4, 8, 12, 16):
            out.add((off, g5, g6, g7))
        if g5 & 1:
            out.add((20, g5, g6, g7))
            g6 += 1
            out.add((24, g5, g6, g7))
        else:
            out.add((28, g5, g6, g7))
            g7 += 1
        out.add((32, g5, g6, g7))
        out.add((36, g5, g6, g7))
    return out


def t_misp(rep, pool, a):
    z = a.seeds[0]
    r, addr, hits, period, d = calibrate("misp", z)
    # a pattern is 2 iterations and the branch path differs, so sweep two whole patterns
    span = 2 * (hits[len(hits) // 2 + 2] - hits[len(hits) // 2])
    base = hits[len(hits) // 2] & ~1
    print(f"misp: loop={addr:#x} pattern={span // 2} halves, H from {base}", flush=True)
    cols = ["Fl_INT", "Fl_JPU", "PC"]
    runs = list(pool.map(lambda h: sim("misp", z, h, cols=cols), range(base, base + span, 2)))
    states, done, stalled, both = misp_states(), [], [], []
    for r in runs:
        m = ENTRY_M.match(r.text)
        d = read_delta(r.delta)
        clash = overlap(high_spans(d["Fl_INT"]), high_spans(d["Fl_JPU"]))
        f = None
        if m:
            f = {"g5": hx(m.group(3)), "g6": hx(m.group(4)), "g7": hx(m.group(5)), "ist": hx(m.group(6)),
                 "m4": hx(m.group(7)), "m0": hx(m.group(8)), "lv": hx(m.group(9)), "loop": hx(m.group(1))}
            done.append((r, f))
        else:
            stalled.append(r.where())
        if clash:
            both.append((r, f))

    def inside(f, shift=0):
        off = pushed(f["m4"], f["m0"]) - shift - f["loop"]
        return (off, f["g5"], f["g6"], f["g7"]) in states
    show = lambda r, f: f"{r.where()} R={pushed(f['m4'], f['m0']):#x} g5={f['g5']} g6={f['g6']} g7={f['g7']}"
    bad = [show(r, f) for r, f in done if not (inside(f) or inside(f, 4))]
    rep.item("10a", "mispredict: return PC (+4 allowed) and registers are on the correct path",
             (not bad) if done else None, f"{len(bad)}/{len(done)} wrong: " + brief(bad, 3) if bad else f"{len(done)} runs")
    bad = [show(r, f) for r, f in done if not inside(f)]
    rep.item("10b", "mispredict: same with the exact return PC", (not bad) if done else None,
             f"{len(bad)}/{len(done)} wrong" if bad else "")
    rep.item("10c", f"mispredict: entry at every phase reaches the end of the handler ({len(runs)} runs)", not stalled,
             f"{len(stalled)} stalled: " + brief(stalled, 8) if stalled else "")
    if both:
        bad = [r.where() + ("" if f else " stalled") for r, f in both if not (f and (inside(f) or inside(f, 4)))]
        rep.item("11", f"Fl_INT and Fl_JPU in the same half ({len(both)} runs hit it)", not bad, brief(bad))
    else:
        rep.item("11", "Fl_INT and Fl_JPU in the same half", None, f"no run hit it in {len(runs)} phases")


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
    wrong = [r.where() for r, _, lv in resumed if lv != 0]
    rep.item("7c", "iret: int-level back to 0", (not wrong) if resumed else None, brief(wrong))


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


TESTS = {"ext": t_ext, "prec": t_prec, "sled": t_sled, "misp": t_misp, "iret": t_iret, "sys": t_sys, "tbl": t_tbl}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-j", type=int, default=9)
    ap.add_argument("-seeds", default="1,2,3", help="zseed list for the prec sweep (others use the first)")
    ap.add_argument("-nobuild", action="store_true")
    ap.add_argument("-snap", action="store_true",
                    help="reuse the circuit copy in program/build/dsim/int/components instead of copying components/ again")
    ap.add_argument("groups", nargs="*", choices=[[], *PROGRAMS], default=[])
    a = ap.parse_args()
    a.seeds = [int(s) for s in a.seeds.split(",")]
    groups = a.groups or PROGRAMS
    OUT.mkdir(parents=True, exist_ok=True)
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
    try:
        with ThreadPoolExecutor(a.j) as pool:
            for g in groups:
                TESTS[g](rep, pool, a)
    except RuntimeError as e:
        rep.show()
        sys.exit(f"stopped: {e}")
    sys.exit(0 if rep.show() else 1)


if __name__ == "__main__":
    main()
