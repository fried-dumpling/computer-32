"""Tests float.asm on the ISA-32 emulator against numpy float32 / exact references.

usage: python fp_test.py [--quick] [--seed N]

Builds fp_harness.asm and float.asm as two objects with tools/bin/dasm, links
them with dlnk, then for each case writes op/args into the harness mailbox, runs the
program and compares the result bit-for-bit.
"""

import os
import random
import struct
import subprocess
import sys
import tempfile
from decimal import Decimal
from fractions import Fraction

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "emu"))
from isa32emu import CPU, EmuError, find_word, R_STACK  # noqa: E402

TOOLS = os.path.normpath(os.path.join(HERE, "..", "..", "tools", "bin"))
MAGIC = 0x7E57F00D
OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_S2F, OP_F2S = range(6)
SENTINEL = [0x5A5A0000 + i for i in range(5, 13)]


def build():
    tmp = tempfile.mkdtemp(prefix="fptest_")
    obj, lib = os.path.join(tmp, "fp_harness.o"), os.path.join(tmp, "float.o")
    out = os.path.join(tmp, "fp_harness.bin")
    for cmd in ([os.path.join(TOOLS, "dasm.exe"), "fp_harness.asm", "-o", obj],
                [os.path.join(TOOLS, "dasm.exe"), "float.asm", "-o", lib],
                [os.path.join(TOOLS, "dlnk.exe"), obj, lib, "-o", out, "-raw"]):
        r = subprocess.run(cmd, cwd=HERE, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout[-2000:], r.stderr[-2000:])
            raise SystemExit(f"build failed: {' '.join(cmd)}")
    return open(out, "rb").read()


class Harness:
    def __init__(self, image):
        self.image = image
        base = find_word(image, MAGIC)
        self.op, self.a, self.b, self.c, self.res, self.str = (base + 4 * i for i in range(1, 7))
        self.max_steps = 0

    def run(self, op, a=0, b=0, c=0, s=None):
        cpu = CPU(self.image)
        cpu.set_word(self.op, op)
        cpu.set_word(self.a, a)
        cpu.set_word(self.b, b)
        cpu.set_word(self.c, c)
        if s is not None:                       # byte string, 0 terminated
            cpu.set_bytes(self.str, s.encode("latin-1") + b"\0")
        for i, v in enumerate(SENTINEL):
            cpu.r[5 + i] = v
        cpu.run()
        self.max_steps = max(self.max_steps, cpu.steps)
        self.end = cpu.word(self.c) - self.str          # fp_str2f end index
        if cpu.r[5:13] != SENTINEL:
            raise EmuError("callee-saved gen[5..12] not preserved")
        if cpu.r[R_STACK] != self.str + 4 * 64 + 4 * 512:
            raise EmuError(f"stack not balanced: {cpu.r[R_STACK]:#x}")
        return cpu.word(self.res), cpu.cstr(self.str, 64), cpu.steps


# ---- references ---------------------------------------------------------

def f32(bits):
    return np.frombuffer(struct.pack("<I", bits), dtype=np.float32)[0]


def bits(x):
    return struct.unpack("<I", np.float32(x).tobytes())[0]


def ref_arith(op, a, b):
    x, y = f32(a), f32(b)
    with np.errstate(all="ignore"):
        r = {OP_ADD: x + y, OP_SUB: x - y, OP_MUL: x * y, OP_DIV: x / y}[op]
    return bits(r)


def frac_to_f32(fr, neg):
    sign = 0x80000000 if neg else 0
    if fr == 0:
        return sign
    e = fr.numerator.bit_length() - fr.denominator.bit_length()
    if Fraction(2) ** e > fr:
        e -= 1
    e = max(e, -126)
    scaled = fr / Fraction(2) ** (e - 23)
    q = scaled.numerator // scaled.denominator
    rem = scaled - q
    if rem > Fraction(1, 2) or (rem == Fraction(1, 2) and q & 1):
        q += 1
    if q == 1 << 24:
        q >>= 1
        e += 1
    if e > 127:
        return sign | 0x7F800000
    if q < 1 << 23:
        return sign | q
    return sign | ((e + 127) << 23) | (q - (1 << 23))


def ref_parse(s):
    """strtod-like scan: returns (value bits, end index)."""
    i = 0
    while i < len(s) and s[i] == " ":
        i += 1
    neg = i < len(s) and s[i] == "-"
    if i < len(s) and s[i] in "+-":
        i += 1
    sign = 0x80000000 if neg else 0
    word = s[i:i + 3].lower()
    if word == "inf":
        return sign | 0x7F800000, i + 3
    if word == "nan":
        return sign | 0x7FC00000, i + 3
    j = i
    while j < len(s) and s[j].isdigit():
        j += 1
    k = j
    if k < len(s) and s[k] == ".":
        k += 1
        while k < len(s) and s[k].isdigit():
            k += 1
    mant = s[i:k]
    if not any(ch.isdigit() for ch in mant):
        return 0, 0
    end, text = k, mant
    if k < len(s) and s[k] in "eE":
        e = k + 1
        if e < len(s) and s[e] in "+-":
            e += 1
        d = e
        while d < len(s) and s[d].isdigit():
            d += 1
        if d > e:
            end, text = d, mant + "e" + s[k + 1:d]
    return frac_to_f32(Fraction(Decimal(text)), neg), end


def ref_s2f(s):
    return ref_parse(s)[0]


def ref_f2s(b, p):
    x = float(f32(b))
    if x != x:
        return "nan"
    return "%.*g" % (p, x)


def is_nan(b):
    return (b & 0x7F800000) == 0x7F800000 and (b & 0x7FFFFF) != 0


# ---- test sets ----------------------------------------------------------

SPECIAL = [0x00000000, 0x80000000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7F800001,
           0x00000001, 0x80000001, 0x007FFFFF, 0x00800000, 0x00800001, 0x7F7FFFFF,
           0xFF7FFFFF, 0x3F800000, 0xBF800000, 0x40000000, 0x3F000000, 0x3DCCCCCD,
           0x4048F5C3, 0x3EAAAAAB, 0x4B800000, 0x33800000, 0x34000000, 0x0C000000]


def rand_bits(rng):
    k = rng.random()
    if k < 0.6:
        return rng.getrandbits(32)
    if k < 0.8:   # near 1.0, for cancellation / carry cases
        return (rng.getrandbits(1) << 31) | ((127 + rng.randint(-3, 3)) << 23) | rng.getrandbits(23)
    if k < 0.9:   # subnormal / tiny
        return (rng.getrandbits(1) << 31) | (rng.randint(0, 2) << 23) | rng.getrandbits(23)
    return rng.choice(SPECIAL)


def rand_decimal(rng):
    nd = rng.randint(1, 15)
    digits = "".join(rng.choice("0123456789") for _ in range(nd))
    sign = rng.choice(["", "", "-", "+"])
    form = rng.random()
    if form < 0.3:
        s = digits
    elif form < 0.6:
        cut = rng.randint(0, nd)
        s = digits[:cut] + "." + digits[cut:]
        if s == ".":
            s = "0."
    else:
        cut = rng.randint(1, nd)
        s = digits[:cut] + "." + digits[cut:] + rng.choice("eE") + \
            rng.choice(["", "+", "-"]) + str(rng.randint(0, 50))
    return sign + s


def main():
    quick = "--quick" in sys.argv
    seed = int(sys.argv[sys.argv.index("--seed") + 1]) if "--seed" in sys.argv else 1
    rng = random.Random(seed)
    h = Harness(build())
    fails = 0

    def report(kind, desc, got, want):
        nonlocal fails
        fails += 1
        if fails <= 40:
            print(f"FAIL {kind}: {desc} -> got {got} want {want}")

    # arithmetic
    n_arith = 150 if quick else 1500
    pairs = [(a, b) for a in SPECIAL for b in SPECIAL] + \
            [(rand_bits(rng), rand_bits(rng)) for _ in range(n_arith)]
    name = {OP_ADD: "add", OP_SUB: "sub", OP_MUL: "mul", OP_DIV: "div"}
    for op in (OP_ADD, OP_SUB, OP_MUL, OP_DIV):
        for a, b in pairs:
            got, _, _ = h.run(op, a, b)
            want = ref_arith(op, a, b)
            if is_nan(want):
                ok = is_nan(got) and (got & 0x00400000)
            else:
                ok = got == want
            if not ok:
                report(name[op], f"{a:08X} {b:08X}", f"{got:08X}", f"{want:08X}")
        print(f"{name[op]}: {len(pairs)} cases done")

    # string -> float
    strs = ["0", "-0", "1", "-1", "0.1", "3.14", "  2.5", "+7", "1e10", "1E-10", ".5", "5.",
            "3.4028235e38", "3.4028236e38", "1e39", "1.17549435e-38", "1.4e-45", "7e-46",
            "7.1e-46", "1e-50", "123456789", "16777217", "0.000001", "inf", "-INF", "nan",
            "340282346638528859811704183484516925440", "0.30000001192092896",
            "1.00000005960464477539", "2.5e-1x", "1.5.3", "1e", "1e+", "2E-x", "-", "",
            "abc", " .", "5.", "infinity", "nAn", "-Inf+1", "inx", "na", "3*4", "-0.0e5)"]
    strs += [rand_decimal(rng) for _ in range(60 if quick else 400)]
    for s in strs:
        got, _, _ = h.run(OP_S2F, s=s)
        want, want_end = ref_parse(s)
        if h.end != want_end:
            report("str2f end", repr(s), h.end, want_end)
        if got != want:
            digits = sum(ch.isdigit() for ch in s.split("e")[0].split("E")[0].lstrip("+-0."))
            report("str2f", repr(s) + (" (>12 digits)" if digits > 12 else ""),
                   f"{got:08X}", f"{want:08X}")
    print(f"str2f: {len(strs)} cases done")

    # float -> string
    vals = SPECIAL + [rand_bits(rng) for _ in range(40 if quick else 300)]
    n = 0
    for b in vals:
        for p in ([9, 6, 1] if b in SPECIAL else [rng.randint(1, 9)]):
            _, got, _ = h.run(OP_F2S, a=b, c=p)
            want = ref_f2s(b, p)
            n += 1
            if got != want:
                report("f2str", f"{b:08X} P={p}", repr(got), repr(want))
    print(f"f2str: {n} cases done")

    # round trip at 9 digits
    for b in [rand_bits(rng) for _ in range(20 if quick else 150)]:
        if is_nan(b):
            continue
        _, s, _ = h.run(OP_F2S, a=b, c=9)
        got, _, _ = h.run(OP_S2F, s=s)
        if got != b:
            report("roundtrip", f"{b:08X} via {s!r}", f"{got:08X}", f"{b:08X}")
    print("roundtrip done")

    print(f"\nmax steps for one call: {h.max_steps}")
    print("ALL PASS" if fails == 0 else f"{fails} FAILURES")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
