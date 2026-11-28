# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Custom 32-bit out-of-order CPU based on the **ISA-32** instruction set architecture, simulated in **hneemann Digital** (circuit simulator). The CPU implements register renaming, a reorder buffer (ROB), branch prediction (BTB), a return address stack (RAS), and load/store buffers.

## Tools

### Assembler

Compiles ISA-32 assembly (`.txt`) to binary. Pre-built binary at `bin/dasm.exe`.

```
bin/dasm.exe <input.txt> [-o <output.o>] [flags]
bin/dlnk.exe <input1.o> [input2.o ...] [-o <output.bin>] [-padbss] [-v]
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
bin/hexdump.exe <file.bin> [-width N] [-bundle N]
```

Default width is 4 bytes per row. Useful for inspecting assembled output.

### Emulator

Instruction-level ISA-32 emulator for testing programs (not the Digital circuit): `tools/emu/isa32emu.py`. Little endian, byte addressed memory, IO at `0x4000000`.

```
python tools/emu/isa32emu.py <program.bin> ["keyboard input"] [-raw]
```

Reads the default dlnk output (8 byte header: start address, length); `-raw` for headerless images. Test scripts (`program/*/..._test.py`) import it as a module (`CPU`, `load_image`, `find_word`).

Strict by default:
- Flag validity is tracked per bit: after `add`/`sub`/`cmp` (and their `i`/`c` forms) all six bits are valid, after shifts/rotates `zero`/`neg`/`pos`/`carry`, after logic ops and `test` only `zero`/`neg`/`pos`. A branch on an undefined bit, or `addc`/`subc`/`shiftlc`/`shiftrc` with an undefined carry, is an error.
- Only `32B` mode is allowed, except `ld`/`st` widths (`32B`/`16L`/`8L`/`8H`) and `set` `S16H`/`16L`.
- Partial `ld` keeps the other bits like the circuit; `st .8L`/`.16L` writes only those bytes. A load after a partial store reads the merged word. Unaligned access is allowed here, but the circuit raises fault 5 (half at an odd address, word not a multiple of 4).
- Carry ops (`addc`/`subc`/`shiftlc`/`shiftrc`) follow the circuit (`document/ISA.md` sections 4/5): after `sub`/`cmp` carry is the borrow, `subc` = a - b - carry, shifts set carry to the OR of the bits shifted out, `shiftlc`/`shiftrc` fill all vacated bits with carry. No interrupt instructions (add them when needed). `rol`/`ror` rotate by the amount mod 32, as on the circuit. `jmp $` is treated as halt.

### Headless circuit simulator (dsim)

`tools/dsim/` runs `components/computer-32-test.dig` (cpu-32 + IPU + memory/IO) without the GUI using `Digital.jar` (key input, terminal output, probe/tunnel CSV dumps, net queries). See `tools/dsim/README.md`. Pass `-` as the program to let the ROMs load their own files (`boot/boot.bin`, `boot/rom.bin`), which is what the current circuit needs (it has no ROM flagged as program memory).

### Rebuilding Tools

Source is in `tools/assembler/`, `tools/linker/`, `tools/common/` (shared headers, included as `../common/`) and `tools/hexdump/` (C++). This is the former `assembler-linker` repo, merged with its history (2026-10-22). Build with `tools/build-asm.cmd`, `build-lnk.cmd`, `build-hex.cmd` (MinGW `g++ -std=c++17`), which write `bin/dasm.exe`, `bin/dlnk.exe`, `bin/hexdump.exe`. Run the build scripts from PowerShell (git-bash `cmd //c` mangles the quoting). Old regression sources: `program/archive/*.txt`. Full assembler/linker syntax, CLI options and error messages: `document/SYNTAX.md`. Internals (pipelines, object format, relocation, name mangling, CLI conventions, known limits, working rules): `document/toolchain.md`. The parser table is cached in `bin/dasm_table.bin`; after changing lexer rules, grammar or the token/AST enums, bump `version` in `assembler.hpp` (`cache` namespace) so it is rebuilt — a stale table silently runs the old grammar. When the grammar, encoding or options change, update `document/SYNTAX.md` too. Building and checking is fine without asking; replacing `bin/` executables and committing only on request.

**Which branch for tool changes** (dsim, dasm, dlnk, hexdump, emu, test scripts): work on the `tools` branch, then merge `tools` into the branches that need it (e.g. `interrupt`, `dev`), when the change
- takes long or is a larger project,
- is an optimization or cleanup unrelated to the circuit (speed, options, refactoring), or
- depends little on the circuit work and will be merged into other branches too.

Only small changes tied to the circuit being worked on (a probe, a new net name, a fix needed to test today's circuit edit) go directly on the current work branch. Never switch the branch of this folder for tool work; keep the current branch's uncommitted circuit files untouched. A one-off commit can be built without a checkout (temporary `GIT_INDEX_FILE` + `commit-tree`). When a checkout is needed (longer tool work, building and testing on `tools`, resolving a merge), use the sibling folders `../computer-32-2nd` and `../computer-32-3rd` as worktree slots: an empty folder is free, `git worktree add ../computer-32-2nd tools` (or 3rd if 2nd is in use; `git worktree list` shows which). Don't create worktrees elsewhere. When finished, `git worktree remove` it and recreate the empty folder.

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
interst <reg>, <imm8>, vec|pri | <id> ; int_vec/int_pri[id] = reg + imm8 (id 0..63)
interld <reg>, vec|pri | <id>        ; reg = int_vec/int_pri[id]
ldexp <reg>, <reg_ex>                ; load from an extended/special register
stexp <reg>, <reg_ex>                ; store to an extended/special register
```

`reg_ex` (only reachable via `ldexp`/`stexp`, not the normal `reg.mode` form): `cpuid` (ex-0), `istack` (ex-1, interrupt stack pointer), `intlevel` (ex-2), `inttmplv` (ex-3, level saved on entry, restored by `iret`), `pbase` (ex-4), `plimit` (ex-5). `panicvec` was removed (2026-11-16 sheet). `syscall` uses `interst` id 1; id 0 is reserved for panic/emergency use.

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

Circuits that nothing references (old versions, unused helpers, `RAS`, `SC-buff`, `BTB-buff`, ...) are in `components/legacy/`; Digital still finds them by file name. Debug modules (`*-debug.dig`) and old test circuits are in `components/debugs/`. Digital only searches the opened circuit's folder and its subfolders, so a circuit in a subfolder cannot use modules in `components/` (opening `debugs/res-station-debug.dig` on its own loses its subcircuits). The old development CPU `cpu-test.dig` (probes, debug modules, its own memory/IO) is in `legacy/` since 2026-11-19 and has to be moved back to `components/` to open; `cpu-32.dig` is edited directly. Debug and non-debug modules are kept in step by hand (the `sync-nondebug.sh` tools were removed).

- **`computer-32.dig`** — top-level system (CPU + memory + IO); **`computer-32-test.dig`** — test system (cpu-32 + `IPU-test.dig` + memory/IO), the circuit dsim runs
  - Structure snapshot of `cpu-32.dig` (pins, start sequence, regions, module tree, unfinished nets, cleanup list): `document/cpu-32.md`.
- **`cpu-32.dig`** — main CPU: fetch, decode, dispatch, execute, commit pipeline
- **`res-station.dig`** (+ `res-station-debug.dig`) — reservation station (instruction window size 16)
- **`ROB-gen.dig`** — reorder buffer for in-order commit; **`ROB-FL.dig`**, **`ROB-PC.dig`**, **`ROB-SC.dig`** — per-field ROB submodules (flags, PC, side-channel data); **`ROB-INT.dig`** — separate, smaller ROB dedicated to interrupt/trap entries
- **`regFile.dig`** — speculative register file with renaming (was `regFile4`; older versions `regFile1`..`regFile3` are in `legacy/`)
- **`BTB.dig`** — branch target buffer (size 16, 2-bit predictor, 12-bit tag, 4-bit index)
- **`legacy/RAS.dig`** — return address stack (4 speculative + 8 architectural modules); not instanced in cpu-32
- **`load-buffer.dig`** / **`store-buffer.dig`** (+ `-debug` variants) — memory operation buffers
- **`store-controller.dig`** — **store-arbiter**: picks the commit/ROB-write order across multiple functional units (ALU, DIR for mov/set/register-ex, LOAD/STORE-BUFFER, etc.), 8-entry. Depends on **`index-compressor.dig`** and **`priority-test8V.dig`** (also `priority-test8`, `priority-test8V8B` variants exist). This is a separate concept from the load/store buffer age matrix below.
  - **`store-controller-half.dig`** — 4-entry version of the same arbiter, used for the interrupt/trap ROB write path (few units currently raise exceptions, so 8 entries were overkill) and reused for the flag-store arbiter (which only uses 2 of its slots today). Depends on `index-compressor-half.dig` / `priority-test4V.dig`.
- **`flush-mux.dig`** — misprediction/exception flush muxing
- **`id-manager.dig`** (+ `-debug`) — instruction ID allocation/tracking
- **`alu-32.dig`** — 32-bit ALU
- **`data-select-4.dig`/`-8.dig`/`-32.dig`** (+ `-4H`/`-8H` half-width variants) — data path selection muxes

### Active Development (dev branch)

- Replacing the queue-based ordering system with an **age matrix** for **load/store buffer** ordering only. Age matrix encodes relative instruction age: bit = 1 means current entry is older than indexed entry. (This is unrelated to the store-arbiter family above.)
- Shrinking the store-arbiter (`store-controller*.dig`) from its original 8-entry design to a 4-entry `-half.dig`, since most consumers (interrupt/trap ROB, flag-store arbiter) don't need the full 8 slots.
- ISA cleanup in progress (see `document/todo.md`): removing NMI, removing the trap-flag, rearranging the ISA. Interrupt/exception support (`syscall`, `iret`, `interst`, `interld`, `ldexp`/`stexp` + extended registers, `ROB-INT.dig`) and `call`-with-base, `block`/`halt` were added recently and are still being wired up.

## Programs (`program/`)

Instruction semantics, flags and ABI: `document/ISA.md`. Assembler syntax: `document/SYNTAX.md`. Build and usage for each program/library: **`program/document/<name>.md`** (`floatingpoint`, `io`, `calc`, `picalc`, `test`). Keep those up to date instead of adding CLAUDE.md files under `program/`.

| Dir | Contents |
|---|---|
| `floatingpoint/` | IEEE-754 binary32 library (`float.hsm` / `float.asm`) + emulator tests |
| `io/` | keyboard / terminal library (`io.hsm` / `io.asm`) + emulator tests |
| `calc/` | float expression calculator (uses floatingpoint + io) |
| `picalc/` | π digits with BBP (uses io) |
| `test/` | small programs for debugging the circuit: `interrupt/` (int_*.asm + int.hsm, run by `tools/dsim/inttest.py`), `memory/` (memtest*, ldstmix, selfflush), and selftest, fill, flushstall, rotate, knownbug, haltbr at the top; list in `program/document/test.md` |
| `archive/` | older `.txt` programs (e.g. `add_terminal.txt`, `prime_terminal.txt`) |
| `build/` | scratch build outputs |

### Conventions

- **Library = header + implementation.** `x.hsm` has constants, macros and `extern` declarations behind an `#ifndef` guard. `x.asm` does `#define X_IMPL 1`, includes the header (so the `extern`s drop out; `extern` and a definition in one file is a dasm error) and exports functions with `global`. Assemble each `.asm` separately and link.
- `#include<...>` paths are relative to the directory of the file containing the `#include` (`calc/calc.asm` uses `../io/io.hsm`).
- Labels in objects are stored as `<file>#label`, so only `global` names can clash. Libraries prefix their labels (`fp_`, `io_`).
- **Link the main object first.** dlnk places text from address 0 in link order, then all data; the header's start address is `_start` (`-start` to change). Libraries start with a jump over themselves in case they are linked first.
- **ABI:** args `gen[1..4]`, return `gen[0]`, `gen[5..12]` callee-saved, `gen[1..4]` and `gen[13..20]` clobbered.
- **Strings:** 1 byte per char, 0-terminated, written as `.data` literals with size > length (`msg: 8, "Hi"`); no alignment needed. Read a byte with `set R.32B, 0` + `ld .8L` (partial loads keep the other bits); the libraries wrap this as `IO_LDB` / `FP_LDB`.
- **Address limit:** code and data reached as `zero.32B, label` (`call`, `jmp`, `ld`) must be below 0x8000, because the offset is a signed 16-bit value.
- **32-bit constants:** `set R.S16H, hi` then `set R.16L, lo` (the order proven on the circuit).
- **Instruction semantics the code relies on** (check these first if the circuit disagrees):
  - a branch uses only the flags of the immediately preceding `cmp`/`cmpi`/`test`/`testi`; `neg` = bit 31 of the result, `pos` = neither zero nor neg; compared values never overflow
  - the libraries don't use the carry/overflow flags, `addc`/`subc`/`shiftlc`/`shiftrc` or rotates (the circuit and the emulator have them, rules in `document/ISA.md`; test: `program/test/carry.asm`)
  - `shiftr`/`shiftri` are logical; register shift amounts are always 0..31
  - immediates are zero-extended (0..65535); `set R.32B, imm` is `R = imm`
  - `push`: `stack -= 4; mem[stack] = v`; `call` pushes the return address
- ASCII only outside comments. Don't use reserved words (`test`, `pos`, `zero`, ...) as labels.
- Toolchain behavior the code relies on (bugs fixed 2026-10-10): 1-argument macros work, an empty `;` comment line is fine (`;` is always a comment), dlnk exits with an error on a broken object, dasm deletes its output file when it fails.

### Running on the circuit

`components/computer-32-test.dig` starts in the boot ROM (`boot/boot.bin`, source `boot/src/boot.asm`). The boot loader reads the image in `boot/rom.bin` (loaded by `rom-test.dig`, autoReload) at ROM `0xFC000000`: word 0 = start address, word 1 = length in bytes, then the program. It copies the program to RAM at 0 and jumps to the start address. So link with the default header straight into the ROM file:

```
dlnk main.o lib.o -o ../boot/rom.bin      ; from program/
```

`.bss` is not copied (add `-padbss` if it must start as zero). Old file headers that say `-o run.txt` are from the earlier preload-program-file flow.

Memory map (`boot/src/boot.asm`): RAM `0x00000000-0x03FFFFFF`, keyboard/terminal `0x04000000` (KEY_IN, KEY_V, KEY_R, TERM_OUT at offsets 0/4/8/12), display `0x08000000` (GraphicCard 160×90, one word per pixel, `0x8000 + RGB555`), ROM `0xFC000000-0xFC3FFFFF`, boot `0xFFFFFF00-0xFFFFFFFF`.

### Known circuit problems that affect programs

Past circuit bugs found with dsim (symptom, cause, fix, status): `document/errors.md`. Check it first when a similar symptom shows up.

- **Natural alignment**: a half (`16L`) needs an even address, a word (`32B`) a multiple of 4; bytes are allowed anywhere. An unaligned `ld`/`st` raises fault 5 (`document/errors.md` #58). The emulator does not check this.
