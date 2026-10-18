# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Custom 32-bit out-of-order CPU based on the **ISA-32** instruction set architecture, simulated in **hneemann Digital** (circuit simulator). The CPU implements register renaming, a reorder buffer (ROB), branch prediction (BTB), a return address stack (RAS), and load/store buffers.

## Tools

### Assembler

Compiles ISA-32 assembly (`.txt`) to binary. Pre-built binary at `tools/bin/dasm.exe`.

```
tools/bin/dasm.exe <input.txt> [-o <output.o>] [flags]
tools/bin/dlnk.exe <input1.o> [input2.o ...] [-o <output.bin>] [-padbss] [-v]
```

Debug flags (can combine):
- `-file` — dump raw source
- `-token` — dump lexer tokens
- `-preproc` — dump preprocessor output
- `-eval` — dump parse/evaluate result
- `-bin` — dump binary encoding
- `-double` — double output mode

### Hexdump

```
tools/bin/hexdump.exe <file.bin> [-width N] [-bundle N]
```

Default width is 4 bytes per row. Useful for inspecting assembled output.

### Emulator

Instruction-level ISA-32 emulator for testing programs (not the Digital circuit): `tools/emu/isa32emu.py`. Little endian, byte addressed memory, IO at `0x4000000`.

```
python tools/emu/isa32emu.py <program.bin> ["keyboard input"] [-raw]
```

Reads the default dlnk output (8 byte header: start address, length); `-raw` for headerless images. Test scripts (`program/*/..._test.py`) import it as a module (`CPU`, `load_image`, `find_word`).

Strict by default:
- ALU instructions other than `cmp`/`test` invalidate the flags; a branch on invalid flags is an error.
- Only `32B` mode is allowed, except `ld`/`st` widths (`32B`/`16L`/`8L`/`8H`) and `set` `S16H`/`16L`.
- Partial `ld` keeps the other bits like the circuit; `st .8L`/`.16L` writes only those bytes (it does not copy the circuit's store D_MD bug).
- No carry, rotate or interrupt instructions (add them when needed). `jmp $` is treated as halt.

### Headless circuit simulator (dsim)

`tools/dsim/` runs `components/cpu-32 test.dig` without the GUI using `Digital.jar` (key input, terminal output, probe/tunnel CSV dumps, net queries). See `tools/dsim/README.md`. Pass `-` as the program to let the ROMs load their own files (`boot/boot.bin`, `boot/rom.bin`), which is what the current circuit needs (it has no ROM flagged as program memory).

### Rebuilding Tools

Source is in `tools/assembler/`, `tools/linker/`, `tools/common/` (shared headers, included as `../common/`) and `tools/hexdump/` (C++), mirrored from `computer 32 (new)/src`. Compile with any C++17-compatible compiler. Entry points: `tools/assembler/main.cpp`, `tools/linker/main.cpp`. Full assembler/linker syntax reference: `tools/SYNTAX.md`. The lexer table is cached in `tools/bin/dasm_table.bin`; after changing lexer rules, bump the table version (or delete the file) so it is rebuilt.

## ISA-32 Assembly Syntax

### Registers

Format: `<reg_id>[<index>].<mode>`

| ID | Description |
|----|-------------|
| `gen[N]` | General-purpose registers (N = 0..24) |
| `reg[N]` | Alias for gen |
| `zero`, `one`, `full` | Constant registers (0, 1, 0xFFFFFFFF) |
| `pc`, `stack`, `flag`, `sbp` | Special registers |

Modes: `32B` (full 32-bit), `16L` (lower 16), `8L`, `8H`, `S16H` (set upper 16 bits), `S16L`, `S8L`, `S8H`

### Memory Access

```
ld.<base_reg> <dest_reg>, <offset16>    ; load from mem[base + offset]
st.<base_reg> <src_reg>, <offset16>     ; store to mem[base + offset]
```

### Control Flow

```
jmp.<flag> <reg>, <imm16>    ; conditional relative jump
ijmp.<flag> <reg>, <imm16>   ; indirect jump (inverted condition)
call <reg>, <imm16>          ; call (reg doubles as base)
ret                          ; return
```

Flag conditions: `zero`, `neg`, `pos`, `carry`, `carry4`, `overflow`, `one` (always), `gen`

### Stack

```
push <reg>
pop <reg>
```

### Interrupts / Exceptions

```
syscall                              ; software interrupt
iret                                 ; return from interrupt
intervec <reg>, <imm8>, <imm8>       ; set interrupt vector entry
interpri <imm8>, <imm8>              ; set interrupt priority
ldexp <reg>, <reg_ex>                ; load from an extended/special register
stexp <reg>, <reg_ex>                ; store to an extended/special register
```

`reg_ex` (only reachable via `ldexp`/`stexp`, not the normal `reg.mode` form): `istack` (interrupt stack pointer), `panicvec` (panic/fault vector), `cpuid`.

### Misc

```
nop                           ; no-op
brk                            ; breakpoint
wait                          ; stall
halt                          ; halt
block                          ; pipeline block
```

### Preprocessor

```asm
#define NAME (expr)           ; constant substitution
#macro NAME argcount          ; macro definition (args referenced as #1, #2, ...)
    ...body...
#end
##localname                   ; unique label per macro expansion (avoids conflicts)
```

### Sections

```asm
.text        ; instructions
.data        ; initialized data: <size_expr>, <value_expr>
.bss         ; uninitialized data: <size_expr>
label:       ; defines a label (usable as an address constant)
```

### Instruction Encoding (32-bit word, stored little endian)

`[opcode 8b][reg_a 8b][reg_b 8b][imm16 or reg_c 8b]` (bits 31..0)

Memory is byte addressed and little endian; `ld`/`st` access width is the register mode (`32B`/`16L`/`8L`/`8H`). Instruction semantics, register modes and ABI: `document/ISA.md` (summary of `document/ISA-32.xlsx`). TODO list: `document/todo.md`.

Opcode table is in `tools/assembler/assembler.hpp` (`instructionBase` map).

## CPU Microarchitecture (Digital circuits in `components/`)

- **`computer-32.dig`** — top-level system (CPU + memory + IO)
- **`cpu-32.dig`** / **`cpu-32 test.dig`** — main CPU: fetch, decode, dispatch, execute, commit pipeline (test variant has extra probes)
- **`res-station.dig`** (+ `res-station 2.dig`, `res-station-debug.dig`) — reservation station (instruction window size 16)
- **`ROB-gen.dig`** (+ `ROB-gen 2.dig`) — reorder buffer for in-order commit; **`ROB-FL.dig`**, **`ROB-PC.dig`**, **`ROB-SC.dig`** — per-field ROB submodules (flags, PC, side-channel data); **`ROB-INT.dig`** — separate, smaller ROB dedicated to interrupt/trap entries
- **`regFile.dig`..`regFile4.dig`** — speculative register file with renaming (iterated versions)
- **`BTB.dig`** / **`BTB-buff.dig`** — branch target buffer (size 16, 2-bit predictor, 12-bit tag, 4-bit index)
- **`RAS.dig`** — return address stack (4 speculative + 8 architectural modules)
- **`load-buffer.dig`** / **`store-buffer.dig`** (+ `-debug` variants) — memory operation buffers
- **`store-controller.dig`** — **store-arbiter**: picks the commit/ROB-write order across multiple functional units (ALU, DIR for mov/set/register-ex, LOAD/STORE-BUFFER, etc.), 8-entry. Depends on **`index-compressor.dig`** and **`priority-test8V.dig`** (also `priority-test8`, `priority-test8V8B` variants exist). This is a separate concept from the load/store buffer age matrix below.
  - **`store-controller-half.dig`** — 4-entry version of the same arbiter, used for the interrupt/trap ROB write path (few units currently raise exceptions, so 8 entries were overkill) and reused for the flag-store arbiter (which only uses 2 of its slots today). Depends on `index-compressor-half.dig` / `priority-test4V.dig`.
- **`SC-buff.dig`** — side-channel/store-controller buffer support
- **`flush-mux.dig`** — misprediction/exception flush muxing
- **`id-manager.dig`** (+ `-debug`) — instruction ID allocation/tracking
- **`alu-32.dig`** — 32-bit ALU
- **`data-select-4.dig`/`-8.dig`/`-32.dig`** (+ `-4H`/`-8H` half-width variants) — data path selection muxes

### Active Development (dev branch)

- Replacing the queue-based ordering system with an **age matrix** for **load/store buffer** ordering only. Age matrix encodes relative instruction age: bit = 1 means current entry is older than indexed entry. (This is unrelated to the store-arbiter family above.)
- Shrinking the store-arbiter (`store-controller*.dig`) from its original 8-entry design to a 4-entry `-half.dig`, since most consumers (interrupt/trap ROB, flag-store arbiter) don't need the full 8 slots.
- ISA cleanup in progress (see `document/todo.md`): removing NMI, removing the trap-flag, rearranging the ISA. Interrupt/exception support (`syscall`, `iret`, `intervec`, `interpri`, `ldexp`/`stexp` + extended registers, `ROB-INT.dig`) and `call`-with-base, `block`/`halt` were added recently and are still being wired up.

## Programs (`program/`)

Instruction semantics, flags and ABI: `document/ISA.md`. Assembler syntax: `tools/SYNTAX.md`. Build and usage for each program/library: **`program/document/<name>.md`** (`floatingpoint`, `io`, `calc`, `picalc`, `test`). Keep those up to date instead of adding CLAUDE.md files under `program/`.

| Dir | Contents |
|---|---|
| `floatingpoint/` | IEEE-754 binary32 library (`float.hsm` / `float.asm`) + emulator tests |
| `io/` | keyboard / terminal library (`io.hsm` / `io.asm`) + emulator tests |
| `calc/` | float expression calculator (uses floatingpoint + io) |
| `picalc/` | π digits with BBP (uses io) |
| `test/` | small programs for debugging the circuit (memtest*, selftest, fill) |
| `archive/` | older `.txt` programs (e.g. `add_terminal.txt`, `prime_terminal.txt`) |
| `build/` | scratch build outputs |

### Conventions

- **Library = header + implementation.** `x.hsm` has constants, macros and `extern` declarations behind an `#ifndef` guard. `x.asm` does `#define X_IMPL 1`, includes the header (so the `extern`s drop out; `extern` and a definition in one file is a dasm error) and exports functions with `global`. Assemble each `.asm` separately and link.
- `#include<...>` paths are relative to the directory of the file passed to dasm (`calc/calc.asm` uses `../io/io.hsm`).
- Labels in objects are stored as `<file>#label`, so only `global` names can clash. Libraries prefix their labels (`fp_`, `io_`).
- **Link the main object first.** dlnk places text from address 0 in link order, then all data; the header's start address is `_start` (`-start` to change). Libraries start with a jump over themselves in case they are linked first.
- **ABI:** args `gen[1..4]`, return `gen[0]`, `gen[5..12]` callee-saved, `gen[1..4]` and `gen[13..20]` clobbered.
- **Strings:** 1 byte per char, 0-terminated, written as `.data` literals with size > length (`msg: 8, "Hi"`); no alignment needed. Read a byte with `set R.32B, 0` + `ld .8L` (partial loads keep the other bits); the libraries wrap this as `IO_LDB` / `FP_LDB`.
- **Address limit:** code and data reached as `zero.32B, label` (`call`, `jmp`, `ld`) must be below 0x8000, because the offset is a signed 16-bit value.
- **32-bit constants:** `set R.S16H, hi` then `set R.16L, lo` (the order proven on the circuit).
- **Instruction semantics the code relies on** (check these first if the circuit disagrees):
  - a branch uses only the flags of the immediately preceding `cmp`/`cmpi`/`test`/`testi`; `neg` = bit 31 of the result, `pos` = neither zero nor neg; compared values never overflow
  - no carry/overflow flags, no `addc`/`subc`/`shiftlc`/`shiftrc`, no rotates
  - `shiftr`/`shiftri` are logical; register shift amounts are always 0..31
  - immediates are zero-extended (0..65535); `set R.32B, imm` is `R = imm`
  - `push`: `stack -= 4; mem[stack] = v`; `call` pushes the return address
- ASCII only outside comments. Don't use reserved words (`test`, `pos`, `zero`, ...) as labels.
- Toolchain behavior the code relies on (bugs fixed 2026-10-10): 1-argument macros work, an empty `;` comment line is fine (`;` is always a comment), dlnk exits with an error on a broken object, dasm deletes its output file when it fails.

### Running on the circuit

`components/cpu-32 test.dig` starts in the boot ROM (`boot/boot.bin`, source `boot/src/boot.asm`). The boot loader reads the image in `boot/rom.bin` (loaded by `rom-test.dig`, autoReload) at ROM `0xFC000000`: word 0 = start address, word 1 = length in bytes, then the program. It copies the program to RAM at 0 and jumps to the start address. So link with the default header straight into the ROM file:

```
dlnk main.o lib.o -o ../boot/rom.bin      ; from program/
```

`.bss` is not copied (add `-padbss` if it must start as zero). Old file headers that say `-o run.txt` are from the earlier preload-program-file flow.

Memory map (`boot/src/boot.asm`): RAM `0x00000000-0x03FFFFFF`, keyboard/terminal `0x04000000` (KEY_IN, KEY_V, KEY_R, TERM_OUT at offsets 0/4/8/12), display `0x08000000` (GraphicCard 160×90, one word per pixel, `0x8000 + RGB555`), ROM `0xFC000000-0xFC3FFFFF`, boot `0xFFFFFF00-0xFFFFFFFF`.

### Known circuit problems that affect programs

- **`halt` runs before an older branch resolves** (2026-10-17, dsim). When a branch is predicted not taken and `halt` follows it, `halt` is issued and stops the CPU while the branch still waits in the reservation station, so the misprediction is never recovered. `calc`, `picalc` and `test/fill` all have `halt` right after a branch and stop at once. Workaround: end with `jmp.one pc.32B, 0` instead of `halt`.
- **`st .8L`/`.16L`/`.8H` writes 4 bytes**: the store D_MD is always 0, so the selected value is zero-extended to a word.
