"""ISA-32 instruction-level emulator (for testing programs, not the Digital circuit).

Semantics follow ISA-32.xlsx / SYNTAX.md and what was measured on the circuit:
  - memory is byte addressed and little endian (dasm / dlnk default -little).
    words, halves and bytes may sit at any address (unaligned is allowed)
  - instruction = 32 bit little endian word: [31:24] op, [23:16] reg_a,
    [15:8] reg_b | imm16 hi, [7:0] imm16 lo
  - register byte = id << 3 | mode
  - immediates are zero extended (0..65535), memory/jump offsets are signed
  - push: stack -= 4, mem[stack] = v ; pop: v = mem[stack], stack += 4
  - call: push(pc + 4), pc = base + off

ld / st use the register mode as the access width (circuit D_MD = mode):
  ld R.32B : R = word            st R.32B : word = R
  ld R.16L : R[15:0] = half      st R.16L : half = R[15:0]
  ld R.8L  : R[7:0]  = byte      st R.8L  : byte = R[7:0]
  ld R.8H  : R[15:8] = byte      st R.8H  : byte = R[15:8]
a partial ld keeps the other bits of R (measured on the circuit : ld .8L
into 0xFFFFFFFF gives 0xFFFFFF22), so clear R first for a zero-extended
byte. a partial st writes only its bytes (measured on the circuit,
2026-10-18).

IO (addr >= IO_BASE) is word wide, any width : KEY_IN, KEY_V read,
KEY_R / TERM_OUT store.

strict=True (default) makes flags valid only right after cmp/cmpi/test/testi;
any other ALU op poisons them, so a branch that depends on flags left by
add/shift/... raises instead of silently guessing the hardware behavior.
Only register mode 32B is accepted, except ld / st (32B / 16L / 8L / 8H)
and `set`. `set` writes only the bits of its mode and keeps the others, as
measured on the circuit: S16H = [31:16], 16L / S16L = [15:0],
8L / S8L = [7:0], 8H / S8H = [15:8].

Unsupported opcodes (carry ops, rotates, interrupts, ...) raise EmuError.

Images: CPU() takes the raw program bytes (dlnk -raw), loaded at 0, pc = 0.
load_image() also reads the default dlnk output with its 8 byte header
(start address, program length).
"""

import struct

IO_BASE = 0x4000000
KEY_IN, KEY_V, KEY_R, TERM_OUT = 0, 4, 8, 12
MEM_SIZE = 0x100000         # emulated RAM (1 MB), accesses beyond it raise

R_SBP, R_ZERO, R_ONE, R_FULL, R_PC, R_STACK, R_FLAG = 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
M32 = 0xFFFFFFFF

# ld / st mode -> (size, shift in the register)
ACCESS = {0: (4, 0), 1: (2, 0), 2: (1, 0), 3: (1, 8)}
# set mode -> (shift, mask) of the bits it writes
SET_FIELD = {1: (0, 0xFFFF), 2: (0, 0xFF), 3: (8, 0xFF),
             4: (16, 0xFFFF), 5: (0, 0xFFFF), 6: (0, 0xFF), 7: (8, 0xFF)}


class EmuError(Exception):
    pass


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def load_image(data, raw=False):
    """(program bytes, start pc) from a dlnk output file."""
    if raw:
        return data, 0
    if len(data) < 8:
        raise EmuError("image shorter than the dlnk header (use raw)")
    start, length = struct.unpack_from("<II", data, 0)
    if length != len(data) - 8:
        raise EmuError(f"header length {length} != {len(data) - 8} (raw image?)")
    return data[8:], start


class CPU:
    def __init__(self, image, strict=True, key_input="", start=0):
        if len(image) > MEM_SIZE:
            raise EmuError("image larger than the emulated memory")
        self.mem = bytearray(MEM_SIZE)
        self.mem[:len(image)] = image
        self.r = [0] * 32
        self.pc = start
        self.flags = None          # (zero, neg, pos) or None
        self.strict = strict
        self.steps = 0
        self.halted = False
        self.keys = list(key_input)
        self.term = []

    # ---- memory -------------------------------------------------------
    def check(self, addr, size):
        if addr + size > MEM_SIZE:
            raise EmuError(f"access {addr:#x} outside memory at pc={self.pc:#x}")

    def load(self, addr, size=4):
        addr &= M32
        if addr >= IO_BASE:
            off = addr - IO_BASE
            if off == KEY_V:
                return 1 if self.keys else 0
            if off == KEY_IN:
                return ord(self.keys[0]) if self.keys else 0
            return 0
        self.check(addr, size)
        return int.from_bytes(self.mem[addr:addr + size], "little")

    def store(self, addr, v, size=4):
        addr &= M32
        if addr >= IO_BASE:
            off = addr - IO_BASE
            if off == KEY_R and self.keys:
                self.keys.pop(0)
            elif off == TERM_OUT:
                self.term.append(chr(v & 0xFF))
            return
        self.check(addr, size)
        self.mem[addr:addr + size] = (v & ((1 << (8 * size)) - 1)).to_bytes(size, "little")

    def word(self, addr):
        return self.load(addr)

    def set_word(self, addr, v):
        self.store(addr, v)

    def set_bytes(self, addr, data):
        self.check(addr, len(data))
        self.mem[addr:addr + len(data)] = data

    def cstr(self, addr, limit=256):
        """0 terminated byte string at addr"""
        end = self.mem.find(0, addr, addr + limit)
        return self.mem[addr:end if end >= 0 else addr + limit].decode("latin-1")

    # ---- registers ----------------------------------------------------
    def rd(self, rb):
        rid, mode = rb >> 3, rb & 7
        if mode != 0:
            raise EmuError(f"read with mode {mode} not supported at pc={self.pc:#x}")
        if rid == R_ZERO:
            return 0
        if rid == R_ONE:
            return 1
        if rid == R_FULL:
            return M32
        if rid == R_PC:
            return self.pc
        return self.r[rid]

    def wr(self, rb, v):
        rid, mode = rb >> 3, rb & 7
        if mode != 0:
            raise EmuError(f"write with mode {mode} not supported at pc={self.pc:#x}")
        if rid in (R_ZERO, R_ONE, R_FULL, R_PC, R_FLAG):
            raise EmuError(f"write to constant/special reg {rid:#x} at pc={self.pc:#x}")
        self.r[rid] = v & M32

    def access(self, rb):
        mode = rb & 7
        if mode not in ACCESS:
            raise EmuError(f"ld/st with mode {mode} not supported at pc={self.pc:#x}")
        return ACCESS[mode]

    def push(self, v):
        self.r[R_STACK] = (self.r[R_STACK] - 4) & M32
        self.store(self.r[R_STACK], v)

    def pop(self):
        v = self.load(self.r[R_STACK])
        self.r[R_STACK] = (self.r[R_STACK] + 4) & M32
        return v

    # ---- flags --------------------------------------------------------
    def set_flags(self, res):
        res &= M32
        z = res == 0
        n = bool(res >> 31)
        self.flags = (z, n, not z and not n)

    def poison(self):
        if self.strict:
            self.flags = None

    def cond(self, f):
        if f == 6:
            return True
        if f not in (0, 1, 2):
            raise EmuError(f"flag {f} not supported at pc={self.pc:#x}")
        if self.flags is None:
            raise EmuError(f"branch on undefined flags at pc={self.pc:#x}")
        return self.flags[f]

    # ---- execution ----------------------------------------------------
    def step(self):
        ins = self.load(self.pc)
        op, a, b, c = ins >> 24, (ins >> 16) & 0xFF, (ins >> 8) & 0xFF, ins & 0xFF
        imm = (b << 8) | c
        npc = self.pc + 4

        alu_rr = {0x00: lambda x, y: x + y, 0x03: lambda x, y: x - y,
                  0x06: lambda x, y: x ^ y, 0x08: lambda x, y: x | y,
                  0x0A: lambda x, y: x & y}
        alu_ri = {0x02: 0x00, 0x05: 0x03, 0x07: 0x06, 0x09: 0x08, 0x0B: 0x0A}

        if op in alu_rr:
            self.wr(a, alu_rr[op](self.rd(a), self.rd(b)))
            self.poison()
        elif op in alu_ri:
            self.wr(a, alu_rr[alu_ri[op]](self.rd(a), imm))
            self.poison()
        elif op in (0x10, 0x12, 0x13, 0x15):
            amt = self.rd(b) if op in (0x10, 0x13) else imm
            if amt >= 32:
                raise EmuError(f"shift by {amt} at pc={self.pc:#x}")
            x = self.rd(a)
            self.wr(a, x << amt if op in (0x10, 0x12) else x >> amt)
            self.poison()
        elif op == 0x16:
            self.set_flags(self.rd(a) - self.rd(b))
        elif op == 0x17:
            self.set_flags(self.rd(a) - imm)
        elif op == 0x18:
            self.set_flags(self.rd(a) & self.rd(b))
        elif op == 0x19:
            self.set_flags(self.rd(a) & imm)
        elif op == 0x1A:                       # set
            rid, mode = a >> 3, a & 7
            if mode == 0:
                self.wr(a, imm)
            else:                              # write the mode's bits, keep the rest
                shift, mask = SET_FIELD[mode]
                self.r[rid] = (self.r[rid] & ~(mask << shift) & M32) | ((imm & mask) << shift)
        elif op == 0x1C:
            self.wr(a, self.rd(b))
        elif op in (0x1D, 0x72):               # block, nop
            pass
        elif op == 0x1E:
            self.wr(a, self.pop())
        elif op == 0x1F:
            self.push(self.rd(a))
        elif 0x20 <= op <= 0x3F:               # ld : partial modes merge
            size, sh = self.access(a)
            v = self.load(self.rd((op - 0x20) << 3) + s16(imm), size)
            rid = a >> 3
            if size == 4:
                self.wr(rid << 3, v)
            else:
                if rid in (R_ZERO, R_ONE, R_FULL, R_PC, R_FLAG):
                    raise EmuError(f"write to constant/special reg {rid:#x} at pc={self.pc:#x}")
                m = ((1 << (8 * size)) - 1) << sh
                self.r[rid] = (self.r[rid] & ~m & M32) | (v << sh)
        elif 0x40 <= op <= 0x5F:               # st : only the mode's bytes
            size, sh = self.access(a)
            self.store(self.rd((op - 0x40) << 3) + s16(imm), self.rd(a & ~7) >> sh, size)
        elif 0x60 <= op <= 0x6F:
            taken = self.cond(op & 7)
            if op >= 0x68:
                taken = not taken
            if taken:
                npc = (self.rd(a) + s16(imm)) & M32
                if npc == self.pc:             # jmp $ : treat as halt
                    self.halted = True
        elif op == 0x70:
            self.push(npc)
            npc = (self.rd(a) + s16(imm)) & M32
        elif op == 0x71:
            npc = self.pop()
        elif op == 0x75:
            self.halted = True
        else:
            raise EmuError(f"opcode {op:#04x} not supported at pc={self.pc:#x}")

        self.pc = npc
        self.steps += 1

    def run(self, max_steps=5_000_000):
        while not self.halted:
            if self.steps >= max_steps:
                raise EmuError(f"step limit {max_steps} reached, pc={self.pc:#x}")
            self.step()
        return self.steps


def find_word(image, value):
    for i in range(0, len(image) - 3, 4):
        if struct.unpack_from("<I", image, i)[0] == value:
            return i
    raise EmuError(f"marker {value:#x} not found")


if __name__ == "__main__":
    import sys
    args = [a for a in sys.argv[1:] if a != "-raw"]
    if not args:
        print("usage: python isa32emu.py <program.bin> [keyboard input] [-raw]")
        print("       the file is the default dlnk output (8 byte header); -raw : no header")
        sys.exit(1)
    img, pc = load_image(open(args[0], "rb").read(), raw="-raw" in sys.argv)
    keys = args[1].encode().decode("unicode_escape") if len(args) > 1 else ""
    cpu = CPU(img, key_input=keys, start=pc)
    try:
        cpu.run()
    finally:
        print("".join(cpu.term), end="")
        print(f"\n[{cpu.steps} steps, pc={cpu.pc:#x}]")
