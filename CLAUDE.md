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

### Rebuilding Tools

Source is in `tools/assembler/`, `tools/linker/`, `tools/common/` (shared headers, included as `../common/`) and `tools/hexdump/` (C++), mirrored from `computer 32 (new)/src`. Compile with any C++17-compatible compiler. Entry points: `tools/assembler/main.cpp`, `tools/linker/main.cpp`. Full assembler/linker syntax reference: `tools/SYNTAX.md`.

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

### Instruction Encoding (32-bit, big-endian)

`[opcode 8b][reg_a 8b][reg_b 8b][imm16 or reg_c 8b]`

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
- ISA cleanup in progress (see `todo.txt`): removing NMI, removing the trap-flag, rearranging the ISA. Interrupt/exception support (`syscall`, `iret`, `intervec`, `interpri`, `ldexp`/`stexp` + extended registers, `ROB-INT.dig`) and `call`-with-base, `block`/`halt` were added recently and are still being wired up.

## Programs

Sample programs in `program/` are written in ISA-32 assembly. `add_terminal.txt` is a complete working example demonstrating IO, function calls, stack usage, and BCD-to-decimal conversion.

IO-mapped memory base: `0x4000000` (KEY_IN, KEY_V, KEY_R, TERM_OUT offsets 0/4/8/12).
