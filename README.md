# cVM

cVM is a small Python-like language runtime built around a custom bytecode compiler, a native C virtual machine, and a standalone Windows executable packer.

cVM compiles a supported subset of Python source into CVM2 bytecode, executes that bytecode in the native VM, and can package programs into standalone `.exe` files.

**Documentation:** <https://keke2di.github.io/c_vm/>

## cVM v0.4.0

v0.4.0 adds functions, scopes, and exceptions. Programs can define nested functions, closures, `lambda`s and decorators, take every parameter kind Python has, and recover from failures with `try`/`except`/`finally`; an unhandled exception prints a CPython-shaped traceback, and every runtime failure raises the exception Python raises. `dict` and `set` became O(1), and `hash()` now matches CPython.

- **`str` (all 47) and `bytes` (all 42) methods:** searching, splitting and joining, case conversion, padding, the `is*` predicates, `translate`/`maketrans`, and `hex`/`fromhex`.
- **`str.format` and `str.format_map`:** automatic and manual field numbering, `[index]`/`[key]` access, `!r`/`!s`/`!a`, and nested specs.
- **`list`, `dict`, `set` methods:** the full surfaces, including a stable `list.sort`, `dict.setdefault`/`fromkeys`, and the complete set algebra.
- **Set and dict operators:** `|`, `&`, `-`, `^`, subset/superset comparisons, `|` on dicts, and the augmented forms.
- **Unicode tables** generated from CPython's `unicodedata` (Unicode 16.0.0) and verified against it for all 1,114,112 code points, so case conversion, character properties, and `repr` escaping match Python.

The packed executable still loads from memory, links the C runtime statically, and imports only `KERNEL32.dll`.

See the [changelog](https://keke2di.github.io/c_vm/CHANGELOG.html) for the full history.

**Recompile required.** The CVM2 container format moved from version 3 to version 5 and the instruction set from 8 to 11 for functions and exceptions, so modules compiled by v0.3.0 or earlier do not load. Packed executables are unaffected: each one carries its own runtime.

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
- Method calls: every public method of `str`, `bytes`, `list`, `dict`, `set`, and `frozenset`
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
- Indexing and slicing
- Unpacking assignment, augmented assignment, and `del`
- Conditional expressions and f-strings
- Iteration: `range`, `enumerate`, `zip`, `map`, `filter`, `reversed`, iterators, `for`/`while ... else`
- Bitwise operators and chained comparisons
- List, set, and dictionary comprehensions with `if` and multiple `for` clauses
- Built-in functions including `len`, `abs`, `min`, `max`, `sum`, `sorted`, `round`, `pow`, `ord`, `chr`, `format`, `isinstance`

It also supports iteration (`range`, `enumerate`, `zip`, `map`, `filter`, `reversed`, `for`/`while ... else`), bitwise and chained-comparison operators, conditional expressions, unpacking assignment, augmented assignment to subscripts, `del`, f-strings, and many built-in functions.

Some constructs are intentionally restricted. For example:

- Default parameter values must be constant expressions.
- `*args`, `**kwargs`, keyword-only parameters, positional-only parameters, and argument unpacking are not supported.
- Generator expressions are not supported; use a list comprehension.
- Nested functions, closures, `lambda`, decorators, classes, exceptions, and imports are not supported.
- Unsupported AST constructs produce compiler errors.

Some supported features behave differently from Python. For example:

- `True` and `False` print as `True`/`False` but are a subtype of `int` and equal `1`/`0`.
- `int` is signed 64-bit; overflow is a runtime error rather than promoting to a big integer.
- Set and `dict`-view iteration follow insertion order, not Python's hash order.

The full list is in [Compatibility](https://keke2di.github.io/c_vm/COMPATIBILITY.html).

## CVM2 format

Compiled modules use the **CVM2** container format, currently format version 5.

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

The v0.4.0 release passes:

```text
123 example tests
12 compiler error tests
14 container validation tests
7 arithmetic error tests
140 runtime error tests
8 runtime stress tests
5 stub tests

309 passed
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

**cVM v0.4.0**

- Functions with closures: nested `def`, `lambda`, decorators, `global`/`nonlocal`, every parameter kind, and comprehensions that scope like Python's
- Exceptions: all 68 built-in types with CPython's hierarchy, `raise`, `try`/`except`/`else`/`finally`, `assert`, and tracebacks with file, line, and function
- `hash()` that matches CPython, and insertion-ordered `dict`/`set` with O(1) lookups
- Slice assignment and deletion, type objects for `zip`/`map`/`filter`/`enumerate`/`reversed`, `object`, and `issubclass`
- All 136 public methods of `str`, `bytes`, `list`, `dict`, `set`, and `frozenset`, plus `str.format`, set and dict operators, and Unicode 16.0.0 tables verified for every code point
- Output matches CPython 3.14 across the supported subset: `repr`/`str`, Unicode strings, shortest-round-trip floats
- Real iteration: `range`, `enumerate`, `zip`, `map`, `filter`, `reversed`, iterators, `for`/`while ... else`
- CVM2 format version 5, opcode table version 11
- Regression and negative tests passing

Future versions will continue to widen the supported Python subset while keeping unsupported behavior explicit.

## License

cVM is distributed under the license contained in [`LICENSE`](LICENSE).
