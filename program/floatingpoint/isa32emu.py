"""ISA-32 instruction-level emulator (for testing programs, not the Digital circuit).

Semantics follow ISA-32.xlsx / SYNTAX.md:
  - instruction = [op][reg_a][reg_b | imm16 hi][imm16 lo], big endian
  - register byte = id << 3 | mode
  - immediates are zero extended (0..65535), memory/jump offsets are signed
  - push: stack -= 4, mem[stack] = v ; pop: v = mem[stack], stack += 4
  - call: push(pc + 4), pc = base + off

strict=True (default) makes flags valid only right after cmp/cmpi/test/testi;
any other ALU op poisons them, so a branch that depends on flags left by
add/shift/... raises instead of silently guessing the hardware behavior.
Only register mode 32B is accepted, except `set` with 32B / S16H.

Unsupported opcodes (carry ops, rotates, interrupts, ...) raise EmuError.
"""

import struct

IO_BASE = 0x4000000
KEY_IN, KEY_V, KEY_R, TERM_OUT = 0, 4, 8, 12

R_SBP, R_ZERO, R_ONE, R_FULL, R_PC, R_STACK, R_FLAG = 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
M32 = 0xFFFFFFFF


class EmuError(Exception):
    pass


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


class CPU:
    def __init__(self, image, strict=True, key_input=""):
        self.mem = {}
        for i in range(0, len(image) - len(image) % 4, 4):
            self.mem[i] = struct.unpack(">I", image[i:i + 4])[0]
        self.r = [0] * 32
        self.pc = 0
        self.flags = None          # (zero, neg, pos) or None
        self.strict = strict
        self.steps = 0
        self.halted = False
        self.keys = list(key_input)
        self.term = []

    # ---- memory -------------------------------------------------------
    def load(self, addr):
        addr &= M32
        if addr & 3:
            raise EmuError(f"unaligned load {addr:#x} at pc={self.pc:#x}")
        if addr >= IO_BASE:
            off = addr - IO_BASE
            if off == KEY_V:
                return 1 if self.keys else 0
            if off == KEY_IN:
                return ord(self.keys[0]) if self.keys else 0
            return 0
        return self.mem.get(addr, 0)

    def store(self, addr, v):
        addr &= M32
        if addr & 3:
            raise EmuError(f"unaligned store {addr:#x} at pc={self.pc:#x}")
        if addr >= IO_BASE:
            off = addr - IO_BASE
            if off == KEY_R and self.keys:
                self.keys.pop(0)
            elif off == TERM_OUT:
                self.term.append(chr(v & 0xFF))
            return
        self.mem[addr] = v & M32

    def word(self, addr):
        return self.mem.get(addr, 0)

    def set_word(self, addr, v):
        self.mem[addr] = v & M32

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
            elif mode == 4:                    # S16H : upper half, keep lower
                self.r[rid] = (imm << 16) | (self.r[rid] & 0xFFFF)
            else:
                raise EmuError(f"set mode {mode} not supported at pc={self.pc:#x}")
        elif op == 0x1C:
            self.wr(a, self.rd(b))
        elif op in (0x1D, 0x72):               # block, nop
            pass
        elif op == 0x1E:
            self.wr(a, self.pop())
        elif op == 0x1F:
            self.push(self.rd(a))
        elif 0x20 <= op <= 0x3F:
            self.wr(a, self.load(self.rd((op - 0x20) << 3) + s16(imm)))
        elif 0x40 <= op <= 0x5F:
            self.store(self.rd((op - 0x40) << 3) + s16(imm), self.rd(a))
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
        if struct.unpack(">I", image[i:i + 4])[0] == value:
            return i
    raise EmuError(f"marker {value:#x} not found")


if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("usage: python isa32emu.py <program.bin> [keyboard input]")
        sys.exit(1)
    img = open(sys.argv[1], "rb").read()
    keys = sys.argv[2].encode().decode("unicode_escape") if len(sys.argv) > 2 else ""
    cpu = CPU(img, key_input=keys)
    try:
        cpu.run()
    finally:
        print("".join(cpu.term), end="")
        print(f"\n[{cpu.steps} steps, pc={cpu.pc:#x}]")
