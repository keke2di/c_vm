# cVM

cVM is a small Python-like language runtime built around a custom bytecode compiler, a native C virtual machine, and a standalone Windows executable packer.

cVM compiles a supported subset of Python source into CVM2 bytecode, executes that bytecode in the native VM, and can package programs into standalone `.exe` files.

**Documentation:** <https://keke2di.github.io/c_vm/>

## cVM v0.2.1

v0.2.1 hardens the standalone executable and extends the type model:

- The packed executable loads its program from memory (no temporary file), links the C runtime statically, and imports only `KERNEL32.dll`. It runs on a clean Windows install with no Visual C++ runtime, and carries a version resource and manifest.
- `bool` and `None` are proper types, `is` / `is not` work, and type objects are supported: `type(x)`, `type(1) is int`, `<class 'int'>`.

v0.2.0 established the function model: first-class functions, keyword arguments, and default parameter values.

See the [changelog](https://keke2di.github.io/c_vm/CHANGELOG.html) for the full history.

Recompile standalone `.cvm` modules after upgrading. Executables packed with an earlier version keep working because they carry their own runtime.

cVM is **not** intended to implement the full Python language. Unsupported Python features are rejected by the compiler or stopped with a runtime error. See [Compatibility](https://keke2di.github.io/c_vm/COMPATIBILITY.html) for the full support matrix and known differences from Python.

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
debug_disasm.py  Bytecode disassembly utility
```

## Supported language subset

The compiler currently supports:

- Integer, floating-point, string, bytes, boolean, and `None` constants
- Variables and assignments
- Local variables inside functions
- Global variables at module scope
- Top-level function definitions
- Keyword arguments and default parameter values
- Functions as values, including built-in functions
- Calls on any expression that evaluates to a function
- Method calls (`list.append`, `dict.get`, `set.add`)
- `if` / `elif` / `else`
- `while`
- `for`, including nested loops
- `break`
- `continue`
- `return`
- `pass`
- Arithmetic operations
- Comparisons
- Boolean `and` / `or` / `not`
- Unary operators
- Membership tests with `in` / `not in`
- Lists
- Tuples
- Sets
- Dictionaries
- Indexing
- Slicing
- `len()`
- List, set, and dictionary comprehensions

Some constructs are intentionally restricted. For example:

- Chained comparisons are not supported.
- `for` loop targets must be simple names.
- `for ... else` and `while ... else` are not supported.
- `range()` requires integer constant arguments and a non-negative step, and only works directly in a `for` statement.
- Comprehensions support a single generator without an `if` condition.
- Multiple assignment targets are not supported.
- Default parameter values must be constant expressions.
- `*args`, `**kwargs`, keyword-only parameters, positional-only parameters, and argument unpacking are not supported.
- Nested functions, closures, `lambda`, decorators, classes, exceptions, and imports are not supported.
- Unsupported AST constructs produce compiler errors.

Some supported features behave differently from Python. For example:

- `True` and `False` print as `True`/`False` but are a subtype of `int` and equal `1`/`0`.
- `/` between two integers performs integer division.
- Comparing values other than numbers, strings, bytes, and tuples is a runtime type error, including `x == None`.
- Lists, tuples, dicts, sets, and bytes print as summaries such as `[list len=3]`.

The full list is in [Compatibility](https://keke2di.github.io/c_vm/COMPATIBILITY.html).

## CVM2 format

Compiled modules use the **CVM2** container format, currently format version 3.

The container contains the information required by the native runtime, including:

- CVM2 format metadata
- Bytecode
- Constants
- Global/name information
- Function metadata, including parameter names and default values
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

Run every suite:

```powershell
python tests/run_all.py
```

`run_all.py` prints only failing tests and the total passed/failed count.

Each suite can also be run on its own. Add `--quiet` to show only failures and the summary.

```powershell
python tests/run_examples.py
python tests/test_compiler_errors.py
python tests/test_container_errors.py
python tests/test_arithmetic_errors.py
python tests/test_runtime_errors.py
python tests/runtime_stress.py
```

The v0.2.1 release passes:

```text
69 example tests
22 compiler error tests
14 container validation tests
8 arithmetic error tests
12 runtime error tests
5 runtime stress tests
5 stub tests

135 passed
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
- Invalid types, including calls on values that are not functions
- Argument binding errors
- Division by zero
- Integer overflow
- Runtime function lookup failures
- Allocation failures

The native runtime has also been exercised with development-time memory-safety testing.

## Development

cVM is intentionally kept separate from **cVM Studio**.

The core runtime and compiler are the focus of this repository. Studio development is planned independently.

## Status

**cVM v0.2.1**

- Standalone executable with no temp file and no Visual C++ runtime dependency
- `bool`, `None`, `is`, and type objects
- First-class functions, keyword arguments, and default parameter values
- Nested loop, nested comprehension, and modulo fixes
- CVM2 format version 3
- Regression and negative tests passing

Future versions will continue to widen the supported Python subset while keeping unsupported behavior explicit.

## License

cVM is distributed under the license contained in [`LICENSE`](LICENSE).
