# SIMP Processor Assembler & Simulator

This repository contains a final project that implements a custom RISC processor named **SIMP**, along with an assembler and a simulator, all written in C. The project was developed to demonstrate concepts in computer architecture, including instruction set design, single-cycle CPU simulation, I/O device handling, and interrupt management.

---

## Teaser:

| ![Circle Image](https://github.com/user-attachments/assets/7671525b-0679-4655-bb8f-f98adb303946) | A white circle rendered on a black background, produced entirely by a custom assembly program running on our simulated RISC processor and displayed in a 256×256 monochrome environment. |
|---|---|

---

## Project Overview

- **Architecture**: SIMP is a 16-register, 32-bit RISC-like processor with **48-bit instructions** and a 12-bit program counter (supporting up to 4096 instructions).  
- **Assembler**: Translates SIMP assembly code into machine instructions (`imemin.txt`) and initializes data memory (`dmemin.txt`) with optional `.word` directives.  
- **Simulator**: Fetches, decodes, and executes instructions from `imemin.txt` against a simulated memory space. It also handles:
  - **Interrupts** (irq0 for timer, irq1 for disk, irq2 for external events)
  - **Disk I/O** (reads/writes entire 512-byte sectors via DMA)
  - **Timer**-driven interrupts
  - **Monitor** writes (256×256 monochrome display)
  - **LEDs** and **7-segment display** outputs
  - Detailed logs and traces of every instruction and hardware access

---

## Key Features

### Single-Cycle Execution
Each instruction completes in one clock cycle, tracked by an internal clock counter.

### Flexible I/O Architecture
- **I/O registers** manage disk commands, timers, interrupts, LEDs, 7-segment display, and monitor pixels.
- Interrupt service routines (ISRs) use a dedicated return register to support nested logic.

### Comprehensive Logging
- **`trace.txt`**: Instruction-by-instruction trace (PC, instruction word, registers).
- **`hwregtrace.txt`**: Logs all reads and writes to I/O registers, with cycle numbers.
- **`leds.txt`**, **`display7seg.txt`**: Real-time changes to LEDs and 7-segment display.
- **`monitor.txt` / `monitor.yuv`**: Final monitor state in text or binary (YUV) format.
- **`regout.txt`**: Final register states upon HALT.

### User-Defined Assembly Programs
Includes sample programs for matrix multiplication, binomial coefficient calculation, drawing shapes on the monitor, and basic disk operations.

---

## Repository Structure

```
├── asm/         # Assembler source code
│   ├── asm.c
│   ├── asm.h
│   ├── Makefile
│   └── README.md
│
├── sim/         # Simulator source code
│   ├── sim.c
│   ├── sim.h
│   ├── Makefile
│   └── README.md
│
├── docs/        # Documentation
│   └── project_report.pdf
│
├── tests/       # Assembly test programs
│   ├── mulmat/
│   ├── binom/
│   ├── circle/
│   ├── disktest/
└── README.md
```

---

## Building and Running

### Assembler
```sh
cd asm/
make
./asm <input_assembly_file> imemin.txt dmemin.txt
```

### Simulator
```sh
cd sim/
make
./sim imemin.txt dmemin.txt diskin.txt irq2in.txt dmemout.txt regout.txt trace.txt hwregtrace.txt cycles.txt leds.txt display7seg.txt diskout.txt monitor.txt monitor.yuv
```

The simulator generates multiple output files (`trace.txt`, `hwregtrace.txt`, `dmemout.txt`, etc.) summarizing the execution.

---

## Sample Programs

### 1. `mulmat.asm`
Multiplies two 4×4 matrices stored at addresses `0x100–0x10F` and `0x110–0x11F`, writing the result to `0x120–0x12F`.

### 2. `binom.asm`
Recursively computes the binomial coefficient `C(n, k)` with values initially at `0x100` (n) and `0x101` (k), storing the result at `0x102`.

### 3. `circle.asm`
Draws a circle (white pixels) in the center of the 256×256 monitor frame buffer, using the radius in `0x100`.

### 4. `disktest.asm`
Demonstrates disk interrupts and DMA by shifting the contents of sectors `0–7` to sectors `1–8`.

---

## Additional Notes

- The system is designed for educational purposes, focusing on CPU/ISA design, interrupt-driven I/O, and low-level hardware simulation.
- All source code is commented to explain the implementation details and data structure usage.
- Documentation in `docs/` for a deeper technical explanation and design.
