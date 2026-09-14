---
title: Development
nav_order: 7
description: "Repository layout, building, testing, and debugging cVM."
---

# Development
{: .no_toc }

How the repository is organized, and how to build, test, and debug cVM.
{: .fs-6 .fw-300 }

<details open markdown="block">
  <summary>
    On this page
  </summary>
  {: .text-delta }
1. TOC
{:toc}
</details>

## Requirements

- Windows
- Python 3
- Microsoft C/C++ build tools

{: .note }
Build from an MSVC Developer Command Prompt or Developer PowerShell. From a regular shell, call `vcvars64.bat` from your Visual Studio installation first.

## Repository layout

| Path | Contents |
|:-----|:---------|
| `compiler/` | Python compiler: AST to bytecode, constant pool, symbol tables, emitter, opcode table, CVM2 writer, command line |
| `vm_c/` | Native VM: loader, interpreter, value model, executable stub, build script |
| `packer/` | Appends a compiled module to the VM stub |
| `examples/` | Example programs, also used as regression tests |
| `tests/` | Test suites |
| `docs/` | This documentation site |
| `gui.py` | Small Tk GUI that compiles and packs a file |
| `debug_disasm.py` | Prints the bytecode the compiler emits |

## Building the VM

```powershell
python vm_c/build.py --release
python vm_c/build.py --debug
```

Both produce `vm_c/stub.exe`. The debug build defines `CVM_DEBUG`, which writes `[CALL]` and `[RETURN]` traces to stderr.

## Compiling and packing

```powershell
python -m compiler.cli examples/test_app.py -o output/test_app.cvm
python -m packer.pack vm_c/stub.exe output/test_app.cvm output/test_app.exe
```

## Inspecting bytecode

`debug_disasm.py` prints each function's instructions:

```powershell
python debug_disasm.py examples/test_defaults.py
```

```text
=== greet(name, punctuation) ===
000: LOAD_CONST 1
005: LOAD_FAST 0
010: BINARY_ADD
011: LOAD_FAST 1
016: BINARY_ADD
017: RETURN
018: LOAD_CONST 2
023: RETURN
```

See [Bytecode](BYTECODE.md) for what each instruction does.

## Tests

Run every suite:

```powershell
python tests/run_all.py
```

`run_all.py` prints only failing tests and the total, and exits with a non-zero status if anything fails.

| Suite | Covers | Tests |
|:------|:-------|------:|
| `tests/run_examples.py` | Compiles, packs, and runs every program in `examples/`, comparing output against CPython | 69 |
| `tests/test_compiler_errors.py` | Unsupported or invalid source is rejected with the expected error | 22 |
| `tests/test_container_errors.py` | Malformed CVM2 containers are rejected | 14 |
| `tests/test_arithmetic_errors.py` | Integer overflow is reported | 8 |
| `tests/test_runtime_errors.py` | Call and argument binding errors are reported | 12 |
| `tests/runtime_stress.py` | Deep recursion and large collections | 5 |
| `tests/test_stub.py` | The packed executable imports only KERNEL32, uses no temp file, and matches CPython | 5 |

Each suite can also run on its own. Pass `--quiet` to print only failures and the summary line.

`runtime_stress.py` uses `vm_c/asan_stub.exe` when present (an AddressSanitizer build) and `vm_c/stub.exe` otherwise.

## Adding a language feature

1. **Compiler.** Compile the new AST node in `compiler/compiler.py`, or reject it with a `CompileError`.
2. **Opcodes.** For a new instruction, add it to `compiler/opcodes.py` and regenerate the C header with `python -m compiler.opcodes`.
3. **VM.** Implement it in `vm_c/vm.c`. Validate operands and types, and set a VM error instead of crashing.
4. **Format.** If the module layout changes, bump `FORMAT_VERSION` in `compiler/crypto.py` and the version check in `vm_c/loader.c`.
5. **Tests.** Add an example with its expected output to `tests/run_examples.py`, and negative cases to the error suites.
6. **Docs.** Update the [Language Reference](LANGUAGE.md), [Compatibility](COMPATIBILITY.md), and [Changelog](CHANGELOG.md).

## Guidelines

Changes should prioritize, in order:

1. Correctness
2. Runtime safety
3. Deterministic error handling
4. Compatibility with the supported subset
5. Regression coverage

{: .important }
Changes to language semantics, bytecode, or the CVM2 layout are compatibility-sensitive. They need tests, a changelog entry, and usually a minor version bump.

Development-time memory-safety tools such as AddressSanitizer are useful when investigating native runtime issues. Their runtime files are not part of the release tree.

## Repository hygiene

Do not commit:

- Native build intermediates (`*.obj`, `*.pdb`, `*.ilk`)
- Generated executables and `.cvm` files
- Python `__pycache__` directories
- The `output/` directory
