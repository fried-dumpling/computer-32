"""Tests pi.asm (decimal digits of pi streamed from BBP) on the ISA-32 emulator.

350..361 has a carry through held 9s (around digit 358), 0..99 has
plain carries (the first at about digit 40).

usage: python pi_test.py            # positions 0..99, 350..361, defaults
       python pi_test.py --quick    # positions 0..19, 100..104

Builds pi.asm and ../io/io.asm, links them, types "pos count" lines and
compares the printed digits with exact digits from Machin's formula.
Also checks the input errors and prints the step count per request.
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PROG = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(PROG, "..", "tools", "emu"))
from isa32emu import CPU  # noqa: E402

TOOLS = os.path.normpath(os.path.join(PROG, "..", "bin"))
ERR = "? pos [count]"


def build():
    tmp = tempfile.mkdtemp(prefix="pitest_")
    obj, lib = os.path.join(tmp, "pi.o"), os.path.join(tmp, "io.o")
    out = os.path.join(tmp, "pi.bin")
    for cmd in ([os.path.join(TOOLS, "dasm.exe"), "picalc/pi.asm", "-o", obj],
                [os.path.join(TOOLS, "dasm.exe"), "io/io.asm", "-o", lib],
                [os.path.join(TOOLS, "dlnk.exe"), obj, lib, "-o", out, "-raw"]):
        r = subprocess.run(cmd, cwd=PROG, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout[-2000:], r.stderr[-2000:])
            raise SystemExit(f"build failed: {' '.join(cmd)}")
    return open(out, "rb").read()


def pi_dec(n):
    """first n decimal digits of pi after the point (Machin, exact integers)"""
    one = 10 ** (n + 20)

    def atan_inv(x):
        s = t = one // x
        k, x2, sign = 1, x * x, -1
        while t:
            t //= x2
            k += 2
            s += sign * (t // k)
            sign = -sign
        return s

    return str(4 * (4 * atan_inv(5) - atan_inv(239)))[1:n + 1]


def run(image, keys, max_steps=1_000_000_000):
    cpu = CPU(image, key_input=keys)
    cpu.run(max_steps)
    return "".join(cpu.term), cpu.steps


def answers(out):
    """output split per request: list of {position: digit}"""
    res = []
    for block in out.split("\n> ")[1:]:
        got = {}
        for m in re.finditer(r"^(\d+): ([0-9 ]+)$", block, re.M):
            pos = int(m.group(1))
            for i, c in enumerate(m.group(2).replace(" ", "")):
                got[pos + i] = c
        res.append((block, got))
    return res


def main():
    quick = "--quick" in sys.argv
    image = build()
    ref = pi_dec(1000)
    fails = 0

    # (pos, count), None = empty line (0 50), "e" = must be an input error
    cases = [(0, 20), (100, 5)] if quick else [(0, 100), (350, 12), None]
    cases += [("x", "e"), ("5 3 2", "e"), ("995 6", "e"), ("1001", "e"), ("0 0", "e0")]
    keys = ""
    for c in cases:
        keys += ("" if c is None else f"{c[0]}" if isinstance(c[0], str) else f"{c[0]} {c[1]}") + "\n"
    keys += "q\n"
    out, steps = run(image, keys)
    print(out)

    for c, (block, got) in zip(cases, answers(out)):
        if c is not None and c[1] == "e":
            if ERR not in block:
                print(f"{c[0]!r}: expected an input error")
                fails += 1
            continue
        if c is not None and c[1] == "e0":           # count 0 : no output
            if block.strip() != c[0]:
                print(f"{c[0]!r}: expected no output, got {block!r}")
                fails += 1
            continue
        pos, count = (0, 50) if c is None else c
        if set(got) != set(range(pos, pos + count)):
            print(f"{c}: printed positions {sorted(got)[:3]}.. ({len(got)})")
            fails += 1
        for p, d in got.items():
            if d != ref[p]:
                print(f"pos {p}: got {d} want {ref[p]}")
                fails += 1
        if (pos == 0) != ("3.\n0: " in block):
            print(f"{c}: '3.' line wrong")
            fails += 1

    for n in (10, 100):
        _, s = run(image, f"0 {n}\nq\n")
        print(f"{n} digits: {s} steps")

    print(f"{steps} steps total")
    print("ALL PASS" if fails == 0 else f"FAIL ({fails})")
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
