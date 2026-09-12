# cVM

cVM is a small Python-like language runtime built around a custom bytecode compiler, a native C virtual machine, and a standalone Windows executable packer.

cVM compiles a supported subset of Python source into CVM2 bytecode, executes that bytecode in the native VM, and can package programs into standalone `.exe` files.

## cVM v0.1.0

The first public release focuses on a stable, deliberately limited language subset and a hardened native runtime.

cVM is **not** intended to implement the full Python language. Unsupported Python features are rejected by the compiler rather than silently interpreted.

## Architecture

```text
Python-like source
        │
        ▼
   Python AST
        │
        ▼
   cVM compiler
        │
        ▼
   CVM2 bytecode
        │
        ├──────────────► .cvm module
        │
        ▼
   Native C VM
        │
        ▼
     program
```

For standalone distribution, the compiled module can be packed into the native VM stub:

```text
.cvm + native VM stub
          │
          ▼
    standalone .exe
```

## Repository layout

```text
compiler/        Python compiler and CVM2 module generation
vm_c/            Native C virtual machine and executable stub
packer/          Standalone EXE packer
examples/        Example programs
tests/           Regression and validation tests
docs/            Technical documentation
gui.py           Development GUI
debug_disasm.py  Bytecode debugging/disassembly utility
```

## Supported language subset

The compiler currently supports:

- Integer, floating-point, string, boolean, and `None` constants
- Variables and assignments
- Local variables inside functions
- Global variables at module scope
- Function definitions
- Function calls
- Method calls
- `if` / `else`
- `while`
- `for`
- `break`
- `continue`
- `return`
- `pass`
- Arithmetic operations
- Comparisons
- Boolean `and` / `or`
- Unary operators
- Membership tests with `in` / `not in`
- Lists
- Tuples
- Sets
- Dictionaries
- Indexing
- Slicing
- `len()`
- List comprehensions
- Set comprehensions
- Dictionary comprehensions

Some constructs are intentionally restricted. For example:

- Chained comparisons are not supported.
- `for` loop targets must be simple names.
- `range()` currently requires integer constant arguments.
- Negative `range()` steps are not supported.
- Comprehensions currently support a single generator without an `if` condition.
- Multiple assignment targets are not supported.
- Unsupported AST constructs produce compiler errors.

The supported subset is intentionally small and explicit.

## CVM2 format

Compiled modules use the **CVM2** container format.

The container contains the information required by the native runtime, including:

- CVM2 format metadata
- Bytecode
- Constants
- Global/name information
- Function metadata
- Function bytecode

CVM2 performs structural and bounds validation before bytecode is executed.

**CVM2 does not provide cryptographic protection for bytecode.** The bytecode payload is stored without encryption.

## Building the native VM

cVM currently targets Windows and uses Microsoft's C toolchain.

Open an **MSVC Developer Command Prompt** or **Developer PowerShell**, then run:

```powershell
python vm_c/build.py --release
```

The release build produces:

```text
vm_c/stub.exe
```

Debug builds can be produced with:

```powershell
python vm_c/build.py --debug
```

Build artifacts are not part of the source release.

## Compiling a program

From the repository root:

```powershell
python -m compiler.cli examples/test_app.py -o output/test_app.cvm
```

If no output path is supplied, the compiler creates a `.cvm` file next to the input source.

For example:

```powershell
python -m compiler.cli examples/test_app.py
```

produces:

```text
examples/test_app.cvm
```

## Creating a standalone EXE

First build the native VM:

```powershell
python vm_c/build.py --release
```

Compile the program:

```powershell
python -m compiler.cli examples/test_app.py -o output/test_app.cvm
```

Then pack it:

```powershell
python -m packer.pack vm_c/stub.exe output/test_app.cvm output/test_app.exe
```

The resulting executable contains the native VM stub and the compiled CVM2 program.

## Running the tests

The example regression suite:

```powershell
python tests/run_examples.py
```

Compiler error tests:

```powershell
python tests/test_compiler_errors.py
```

CVM2/container validation tests:

```powershell
python tests/test_container_errors.py
```

Arithmetic overflow/error tests:

```powershell
python tests/test_arithmetic_errors.py
```

Runtime stress tests:

```powershell
python tests/runtime_stress.py
```

The v0.1.0 release candidate currently passes:

```text
55 example tests
4 compiler error tests
9 container validation tests
8 arithmetic error tests

76 passed
0 failed
```

## Runtime safety

The native VM validates loaded CVM2 data before execution and reports runtime errors through explicit VM error codes.

The runtime handles conditions including:

- Invalid CVM2 metadata
- Out-of-bounds container data
- Invalid opcodes
- Invalid constant/function/local references
- Stack errors
- Invalid types
- Division by zero
- Integer overflow
- Runtime function lookup failures
- Allocation failures

The native runtime has also been exercised with development-time memory-safety testing.

## Development

cVM is intentionally kept separate from **cVM Studio**.

The core runtime and compiler are the focus of the v0.1.0 release. Studio development is planned independently after the core release.

## Status

**cVM v0.1.0 — first public release**

The v0.1.0 scope is intentionally conservative:

- Supported language subset is frozen.
- Runtime behavior is stabilized.
- CVM2 loading is validated.
- Native runtime error handling has been hardened.
- Release build tooling is in place.
- Regression and negative tests are passing.

Future versions can expand the language and runtime without changing the deliberately limited scope of the first public release.

## License

cVM is distributed under the license contained in [`LICENSE`](LICENSE).
