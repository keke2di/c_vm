---
title: Changelog
nav_order: 9
description: "Changes in each cVM release."
---

# Changelog
{: .no_toc }

Changes in each cVM release.
{: .fs-6 .fw-300 }

## v0.2.1
{: .d-inline-block }

Latest
{: .label .label-green }

### Added

- `bool` is a distinct type: `True` and `False` print as `True`/`False` while still behaving as the integers `1` and `0` in arithmetic, indexing, slicing, and as dict keys.
- `None` is a single shared object, so `is` and `is not` work; `None is None` is `True`.
- Type objects: `type(x)`, `type(1) is int`, and reprs such as `<class 'int'>`. `int`, `float`, `str`, `bool`, `list`, `tuple`, `dict`, `set`, `bytes`, and `type` are the type objects themselves.

### Changed

- The packed executable loads its program from memory instead of writing it to a temporary file, and links the C runtime statically. It now imports only `KERNEL32.dll` and runs on a clean Windows install with no Visual C++ runtime.
- Packed executables carry a version resource and an application manifest.
- Comparisons, membership tests, and `not` produce real `bool` values (`print(1 == 1)` shows `True`).
- A `return` statement outside a function is a compile error. Previously a module could `return` a value that the executable printed; use `print()` for output.

### Internal

- The native VM source is split into per-area files (values, numeric/comparison/sequence operations, calls, built-ins, type objects, loader, stub, platform). No behavior or performance change; the VM is measurably as fast.

### Compatibility

{: .important }
Recompile any standalone `.cvm` modules. Executables packed with an earlier version keep working because each carries its own runtime.

## v0.2.0
{: .d-inline-block }

Released
{: .label }

### Added

- Functions are first-class values. User-defined and built-in functions can be assigned, passed, returned, and stored in collections.
- Any expression that evaluates to a function can be called, such as `choose(1)(3, 4)` or `ops[0](x)`.
- Keyword arguments, using the new `CALL_KW` instruction.
- Default parameter values, limited to constant expressions.
- [Compatibility](COMPATIBILITY.md) page with the full support matrix.
- `tests/run_all.py`, which runs every suite and prints only failures, plus `--quiet` for each suite.
- `tests/test_runtime_errors.py` for call and argument binding errors.
- `debug_disasm.py` takes the source file to disassemble as an argument.

### Changed

- CVM2 format version 3: function records now carry parameter names and default values.
- `CALL` finds the callee on the stack below its arguments, and the callee is evaluated first.
- `print` returns `None`.
- A top-level `def` with a built-in's name replaces that built-in.
- Code that was silently ignored is now a compile error: decorators, `for ... else`, `while ... else`, `**` in dict literals, type parameters, `*args`, `**kwargs`, keyword-only and positional-only parameters, and argument unpacking.
- Python syntax errors are reported as `compile error:` instead of a traceback.
- The GUI no longer describes cVM as code protection.

### Fixed

- A `for` loop over a collection nested inside another one ended the outer loop early.
- Nested comprehensions produced wrong results, and set and dict comprehensions could not iterate over dict keys.
- `%` with a negative operand used C remainder rules instead of Python's.
- `print` with several arguments printed them in reverse order.
- Sets printed as `{set len=N]`.
- A double free when the VM ran out of memory during startup.
- `tests/runtime_stress.py` required an AddressSanitizer build of the VM.

### Compatibility

{: .important }
Modules compiled with v0.1.0 must be recompiled. Executables packed with v0.1.0 keep working because each one carries its own runtime.

## v0.1.0

First public release.

- Compiler for a deliberately limited Python subset
- CVM2 container format with structural validation
- Native C VM with explicit runtime error codes and integer overflow checks
- Standalone Windows executable packer
- Example, compiler error, container validation, and arithmetic error test suites
