"""Tests io.asm on the ISA-32 emulator (tools/emu/isa32emu.py).

usage: python io_test.py

Builds io_harness.asm and io.asm as two objects, links them, runs the
program with a fixed keyboard input and compares the terminal output.
"""

import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "emu"))
from isa32emu import CPU  # noqa: E402

TOOLS = os.path.normpath(os.path.join(HERE, "..", "..", "tools", "bin"))

VALS = [0, 7, -1, 10, 1000000000, 2147483647, -2147483648, -123456789, 0x1234ABCD]
# line 1: backspace and overflow (max 4 chars), line 2: backspace on empty + '\r'
KEYS = "ab\bcdefg\n" + "\b\bx\r" + "Z"


def build():
    tmp = tempfile.mkdtemp(prefix="iotest_")
    obj, lib = os.path.join(tmp, "io_harness.o"), os.path.join(tmp, "io.o")
    out = os.path.join(tmp, "io_harness.bin")
    for cmd in ([os.path.join(TOOLS, "dasm.exe"), "io_harness.asm", "-o", obj],
                [os.path.join(TOOLS, "dasm.exe"), "io.asm", "-o", lib],
                [os.path.join(TOOLS, "dlnk.exe"), obj, lib, "-o", out, "-raw"]):
        r = subprocess.run(cmd, cwd=HERE, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout[-2000:], r.stderr[-2000:])
            raise SystemExit(f"build failed: {' '.join(cmd)}")
    return open(out, "rb").read()


def expected():
    out = "".join(f"{v} {v & 0xFFFFFFFF:08X}\n" for v in VALS)
    out += "hi2hi\n2\n"
    out += "abcd4\nHello5\nxyz3\n0\n" + "Hello\n5\n"      # byte strings, println
    out += "> ab\n> acde\n" + "4[acde]\n"
    out += "x\n" + "1[x]\n"
    out += "Z0"
    return out


def main():
    cpu = CPU(build(), key_input=KEYS)
    cpu.run()
    got = "".join(cpu.term)
    want = expected()
    ok = got == want
    if not ok:
        print("got :", repr(got))
        print("want:", repr(want))
    print(f"{cpu.steps} steps")
    print("ALL PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
