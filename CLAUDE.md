# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Custom 32-bit out-of-order CPU based on the **ISA-32** instruction set architecture, simulated in **hneemann Digital** (circuit simulator). The CPU implements register renaming, a reorder buffer (ROB), branch prediction (BTB), a return address stack (RAS), and load/store buffers.

## Tools

### Assembler

Compiles ISA-32 assembly (`.txt`) to binary. Pre-built binary at `tools/bin/dasm.exe`.

```
tools/bin/dasm.exe <input.txt> <output.bin> [flags]
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

### Rebuilding Tools

Source is in `tools/assembler src/` (C++) and `tools/hexdump src/`. Compile with any C++17-compatible compiler. The assembler entry point is `tools/assembler src/main.cpp`.

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
call <reg>, <imm16>          ; call
ret                          ; return
```

Flag conditions: `zero`, `neg`, `pos`, `carry`, `carry4`, `overflow`, `one` (always), `gen`

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

Opcode table is in `tools/assembler src/assembler.hpp` (`instructionBase` map).

## CPU Microarchitecture (Digital circuits in `components/`)

- **`computer-32.dig`** — top-level system (CPU + memory + IO)
- **`cpu-32.dig`** — main CPU: fetch, decode, dispatch, execute, commit pipeline
- **`res-station.dig`** — reservation station (instruction window size 16)
- **`ROB-gen.dig`** — reorder buffer for in-order commit
- **`regFile*.dig`** — speculative register file with renaming
- **`BTB.dig`** / **`BTB-buff.dig`** — branch target buffer (size 16, 2-bit predictor, 12-bit tag, 4-bit index)
- **`RAS.dig`** — return address stack (4 speculative + 8 architectural modules)
- **`load-buffer.dig`** / **`store-buffer.dig`** — memory operation buffers
- **`id-manager.dig`** — instruction ID allocation/tracking
- **`alu-32.dig`** — 32-bit ALU

### Active Development (dev branch)

Currently replacing the queue-based ordering system with an **age matrix** for load/store buffer ordering. Age matrix encodes relative instruction age: bit = 1 means current entry is older than indexed entry.

## Programs

Sample programs in `program/` are written in ISA-32 assembly. `add_terminal.txt` is a complete working example demonstrating IO, function calls, stack usage, and BCD-to-decimal conversion.

IO-mapped memory base: `0x4000000` (KEY_IN, KEY_V, KEY_R, TERM_OUT offsets 0/4/8/12).
