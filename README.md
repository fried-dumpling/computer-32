# Description
__Custom 32bit CPU__ based on my DPI __ISA-32__ architecture instruction set.
the project is running on _hneemann Digital_ simulator

# Specification
- __32 bit__ registers, ALU and data/address lines
- __Out of Order__ execution: register renaming, reservation station (instruction window of size 16), reorder buffer with in-order commit
- branch prediction (BTB, 16 entries, 2-bit predictor) and call/ret prediction (return address stack)
- load/store buffers ordered by an age matrix
- byte addressed, little endian memory; boot ROM that loads a program from an external ROM into RAM

# ISA-32
Full reference: [`document/ISA.md`](document/ISA.md) (summary of `document/ISA-32.xlsx`, the sheet is the source of truth).
Assembly syntax: [`document/SYNTAX.md`](document/SYNTAX.md).

## Instruction format
Every instruction is one 32-bit word, stored little endian.

| bits 31:24 | 23:16 | 15:8 | 7:0 |
|---|---|---|---|
| opcode | operand 1 | operand 2 | operand 3 |

16-bit immediates and offsets use bits 15:0. A register operand is `id << 3 | mode`.
Opcodes `00`–`74` are user instructions, `75`–`FF` are kernel instructions (`80`–`FF` reserved).

## Registers

| id | name | ABI / alias |
|---|---|---|
| 00 | `gen[0]` | return |
| 01 | `gen[1]` | arg, ret2 |
| 02–04 | `gen[2]`–`gen[4]` | arg |
| 05–0C | `gen[5]`–`gen[12]` | callee-saved |
| 0D–14 | `gen[13]`–`gen[20]` | caller-saved |
| 15–16 | `gen[21]`–`gen[22]` | at0, at1 (assembler / linker temporaries) |
| 17 | `gen[23]` | kt (kernel temporary) |
| 18 | `gen[24]` | addr (address base, preserved) |
| 19 | `sbp` | frame pointer (preserved) |
| 1A–1C | `zero`, `one`, `full` | constants 0, 1, 0xFFFFFFFF |
| 1D–1F | `pc`, `stack`, `flag` | special |

Calling convention ([`ISA.md` "호출 규약 (ABI)"](document/ISA.md)): arguments 1–4 in `gen[1]`–`gen[4]`, the rest pushed on the stack by the caller (last first, caller cleans up); return in `gen[0]` (up to 8 bytes in `gen[0]` + `gen[1]`); `stack` grows down, 4-byte aligned, no red zone. `syscall` takes its number in `gen[0]`.

Extended registers, reached only through `ldexp`/`stexp`: `cpu-id`, `panic-vec`, `istack`, `int-level`, `p-base`, `p-limit`.

Register modes select the bits used: `32B`, `16L`, `8L`, `8H`, `S16H`, `S16L`, `S8L`, `S8H`.
For `ld`/`st` the mode is the access width (`32B` / `16L` / `8L` / `8H`); a partial load keeps the other bits.

Flags: `zero`, `neg`, `pos`, `carry`, `carry4`, `overflow`, `one` (always 1), `gen`. Conditional jumps test one of these 8 bits.

## Instructions

| group | instructions |
|---|---|
| ALU | `add` `addc` `addi` `sub` `subc` `subi` `bxr` `bxri` `bor` `bori` `bnd` `bndi` `rol` `roli` `ror` `rori` `shiftl` `shiftlc` `shiftli` `shiftr` `shiftrc` `shiftri` `cmp` `cmpi` `test` `testi` |
| register / stack | `set` `sets` `mov` `block` `push` `pop` |
| memory | `ld.<base>` `st.<base>` (address = base register + signed 16-bit offset) |
| control flow | `jmp.<flag>` `ijmp.<flag>` `call` `ret` `nop` `brk` `wait` (targets are base register + offset, not PC relative) |
| kernel | `halt` `syscall` `iret` `intervec` `interpri` `ldexp` `stexp` `pctl` `getabs` `cache` `atom` |

The assembler keeps `jmp` / `ijmp` / `brk` for the sheet's `jp` / `jpi` / `break`.

# Repository

| path | contents |
|---|---|
| `components/` | Digital circuits. `cpu-test.dig` is the CPU used for testing |
| `boot/` | boot ROM program (`boot/src/boot.asm`) and the external ROM image `boot/rom.bin` |
| `program/` | programs and libraries: floating point, keyboard/terminal io, calculator, pi digits, circuit tests |
| `tools/` | assembler, linker and hexdump sources (C++), ISA-32 emulator (`tools/emu`), headless circuit runner (`tools/dsim`) |
| `bin/` | built `dasm.exe`, `dlnk.exe`, `hexdump.exe` |
| `document/` | ISA, assembler syntax, toolchain internals, TODO |

# Quick start
```
cd program
../bin/dasm.exe picalc/pi.asm -o pi.o
../bin/dasm.exe io/io.asm -o io.o
../bin/dlnk.exe pi.o io.o -o ../boot/rom.bin
```
Open `components/cpu-test.dig` in Digital and run: the boot ROM copies `boot/rom.bin` into RAM and jumps to `_start`.
`python tools/emu/isa32emu.py <program.bin>` runs the same image in the instruction-level emulator.
See `program/document/<name>.md` for each program's build steps.
