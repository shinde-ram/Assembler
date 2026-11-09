# IA-32 Two-Pass Assembler

A C++ implementation of a **Two-Pass Assembler** for the IA-32 (32-bit x86) architecture. It translates assembly language instructions into machine code and generates an ELF32 relocatable object file.

## Features

- Two-pass assembly process.
- Instruction encoding using an opcode table.
- Support for 32-bit general-purpose registers.
- ModR/M byte calculation.
- SIB byte calculation for memory addressing.
- Immediate value encoding.
- Relative jump and branch instruction handling.
- Symbol table management for labels and variables.
- ELF32 relocatable object file generation.
- Object file inspection using `readelf` and `objdump`.

## Project Structure

```text
.
├── Main.cpp
├── lib.cpp
├── lib.h
├── validateCode.cpp
├── calculateModRM.cpp
├── optable.txt
├── sampleAsmCode.txt
├── run.sh
│
└── objectCreation/
    ├── elfWriter.cpp
    └── elfWriter.h
```

## Requirements

- Linux operating system
- G++ compiler
- GNU Binutils (`readelf`, `objdump`)

Install the required tools on Ubuntu:

```bash
sudo apt install g++ binutils
```

## How to Run

First, give execution permission to the script:

```bash
chmod +x run.sh
```

The `run.sh` script provides four execution options.

### 1. Compile

Compile the assembler source files:

```bash
./run.sh compile
```

This generates the `assembler` executable.

### 2. Run

Execute the assembler with an input assembly file:

```bash
./run.sh run sampleAsmCode.txt
```

If no input file is specified, `sampleAsmCode.txt` is used by default.

### 3. Display Object File

Display the generated ELF object file information:

```bash
./run.sh display
```

This displays:

- ELF header information
- Section headers
- Symbol table
- Disassembled machine code

### 4. Compile, Run and Display

Perform all three operations in sequence:

```bash
./run.sh all sampleAsmCode.txt
```

This compiles the assembler, executes it using the specified input file, and displays the generated object file information.

## Output

The assembler generates an ELF32 relocatable object file:

```text
output.o
```

You can inspect it independently using:

```bash
readelf -h output.o
readelf -S output.o
readelf -s output.o
readelf -r output.o
objdump -d -Mintel output.o
```

## Technologies Used

- **Programming Language:** C++
- **Target Architecture:** IA-32 (x86 32-bit)
- **Object File Format:** ELF32
- **Development Environment:** Linux
- **Inspection Tools:** GNU Binutils

## Author

Developed as an academic project to understand assembler design, instruction encoding, symbol resolution, and ELF object file generation.