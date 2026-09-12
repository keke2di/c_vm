# Development

## Requirements

cVM currently targets Windows for its native runtime.

Development requires:

- Python
- An MSVC Developer Command Prompt or Developer PowerShell
- A working Microsoft C/C++ build environment

## Native VM build

From the repository root:

```powershell
python vm_c/build.py --release
```

For a development/debug build:

```powershell
python vm_c/build.py --debug
```

The normal release build produces:

```text
vm_c/stub.exe
```

Build outputs and intermediate compiler artifacts should not be committed.

## Compiler

Compile an example module with:

```powershell
python -m compiler.cli examples/test_app.py -o output/test_app.cvm
```

The compiler produces a CVM2 module containing the compiled program.

## Packing

A compiled module can be embedded into the native VM stub:

```powershell
python -m packer.pack vm_c/stub.exe output/test_app.cvm output/test_app.exe
```

The resulting executable contains the native runtime and the compiled CVM2 program.

## Tests

Run the example regression suite:

```powershell
python tests/run_examples.py
```

Run compiler-negative tests:

```powershell
python tests/test_compiler_errors.py
```

Run CVM2 validation tests:

```powershell
python tests/test_container_errors.py
```

Run arithmetic error tests:

```powershell
python tests/test_arithmetic_errors.py
```

Runtime stress tests:

```powershell
python tests/runtime_stress.py
```

The v0.1.0 release test set currently passes 76 tests across the example, compiler-error, container-error, and arithmetic-error suites.

## Development rules for v0.1.x

The v0.1 release line should prioritize:

1. Correctness
2. Runtime safety
3. Deterministic error handling
4. Compatibility with the supported language subset
5. Regression coverage

Changes that alter language semantics, bytecode compatibility, or the CVM2 layout should be treated as compatibility-sensitive work.

## Native runtime safety

The native VM should be tested with both normal regression cases and malformed-input cases.

Development-time memory-safety tools such as AddressSanitizer may be used when investigating native runtime issues. Their runtime files are development artifacts and are not part of the normal release tree.

## Repository hygiene

Do not commit:

- Native compiler intermediates
- Generated executables
- Python `__pycache__` directories
- Temporary output files
- Local development artifacts

The public repository should contain source, tests, examples, documentation, and the files required to reproduce a release.
