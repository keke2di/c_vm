---
title: Changelog
nav_order: 9
description: "Changes in each cVM release."
---

# Changelog
{: .no_toc }

Changes in each cVM release.
{: .fs-6 .fw-300 }

## v0.3.0
{: .d-inline-block }

Latest
{: .label .label-green }

Complete `str` and `bytes` method surfaces, backed by generated Unicode tables. Every public method of `str`, `bytes`, `list`, `dict`, `set`, and `frozenset` is now implemented — 136 in total — and matches CPython 3.14 for the supported argument forms.

### Added

- **Unicode support tables** generated from CPython's own `unicodedata` (Unicode 16.0.0) and verified against it for all 1,114,112 code points: character properties, case mapping including special casing (`"ß".upper()` is `"SS"`, `"ﬃ".upper()` is `"FFI"`), and decimal digit values.
- **`str` methods (all 47).** Searching (`find`, `rfind`, `index`, `rindex`, `count`, `startswith`, `endswith`), editing (`replace`, `strip`, `lstrip`, `rstrip`, `removeprefix`, `removesuffix`), splitting and joining (`split`, `rsplit`, `splitlines`, `join`, `partition`, `rpartition`), case conversion (`upper`, `lower`, `casefold`, `capitalize`, `title`, `swapcase`), padding (`center`, `ljust`, `rjust`, `zfill`, `expandtabs`), the twelve `is*` predicates, and `translate`/`maketrans`.
- **`str.format` and `str.format_map`**, with automatic and manual field numbering, `[index]` and `[key]` access, `!r`/`!s`/`!a` conversions, nested format specs (`"{:{}}"`), and the full format-spec mini-language already used by f-strings.
- **`bytes` methods (all 42)**, mirroring `str` on byte indices with ASCII-only case rules, plus `hex()`, `fromhex()`, `translate`, and `maketrans`. `find`, `rfind`, `index`, `rindex`, and `count` also accept an integer.
- **`list`, `dict`, and `set` methods.** `list.extend/insert/pop/remove/clear/index/count/reverse/copy/sort`; `dict.pop/popitem/setdefault/update/clear/copy/fromkeys`; the full `set` and `frozenset` algebra (`union`, `intersection`, `difference`, `symmetric_difference`, their `_update` forms, `issubset`, `issuperset`, `isdisjoint`, `add`, `remove`, `discard`, `pop`).
- **Set and dict operators.** `|`, `&`, `-`, `^` on sets and frozensets, subset and superset comparisons with `<`, `<=`, `>`, `>=`, `|` on dicts, and the augmented forms. The result type follows the left operand.
- **Methods on type objects.** `str.maketrans(...)`, `bytes.fromhex(...)`, and `dict.fromkeys(...)` can be called on the type as well as on an instance.

### Changed

- `repr` now escapes non-printable non-ASCII characters (`repr("a​b")` is `'a\\u200bb'`), while printable non-ASCII text stays as itself (`repr("café")` is `'café'`).
- `int()` and `float()` accept the 760 Unicode decimal digits, so `int("１２")` is `12`. `bytes` input keeps the ASCII-only rules.
- `list.sort` is stable and shares its implementation with `sorted`, so `key=` and `reverse=` behave identically.

### Compatibility

{: .important }
**No recompile needed.** The CVM2 container format stays at version 3 and the instruction set is unchanged since v0.2.2, so modules compiled with v0.2.2 load and run on this release unmodified. This is the first cVM release that does not require recompiling `.cvm` modules. Programs that print non-printable non-ASCII text through `repr` will see the new escaping.

## v0.2.2
{: .d-inline-block }

Released
{: .label }

A large step toward the Python subset: real iteration, more operators, more syntax, and many built-ins. `print` and `str` now match CPython's `repr`/`str` output, including Unicode text and shortest-round-trip floats.

### Added

- **Iteration.** A real iterator protocol. `for` and `while ... else` / `for ... else`; `iter()` and `next()` (with an optional default); lazy `range` objects (`range` is a callable type with dynamic arguments, negative steps, indexing, `len`, `in`, and `==`); lazy `enumerate`, `zip`, `map`, `filter`, and `reversed` that yield tuples. `zip`/`map` accept `strict=`.
- **Operators.** Bitwise `& | ^ << >> ~`; sequence concatenation and repetition for `list`, `tuple`, `str`, and `bytes`; chained comparisons (`a < b < c`), evaluated once and short-circuiting.
- **Syntax.** Conditional expressions (`a if c else b`); unpacking assignment (`a, b = ...`, nested, one starred target, `a = b = c`, and `for` targets); augmented assignment to subscripts (`x[i] += 1`); `del` for names and subscripts; comprehensions with multiple `for` clauses, `if` filters, and tuple targets; f-strings with the full format-spec mini-language and `!r`/`!s`/`!a` conversions.
- **Built-in functions.** `abs`, `divmod`, `pow` (including 3-argument modular `pow`), `round`, `sum`, `min`, `max`, `sorted`, `any`, `all`, `isinstance`, `callable`, `id`, `ord`, `chr`, `bin`, `oct`, `hex`, `format`, `ascii`, `repr`, `enumerate`, `zip`, `map`, `filter`, `reversed`, `iter`, `next`. `len` is now also a first-class value.
- **Types and constructors.** `list`, `tuple`, `set`, `dict`, and `bytes` build from any iterable; `frozenset`; `dict.keys()`, `dict.values()`, and `dict.items()` view objects. `int(x, base)` and `float(x)` parse like CPython (underscores, `0x`/`0o`/`0b` prefixes, `inf`/`nan`, Unicode whitespace). `bytes`/`str` encode and decode with `utf-8`, `utf-8-sig`, `ascii`, and `latin-1` (`str.encode`, `bytes.decode`).
- **Keyword arguments** now work for built-in functions, type constructors, and method calls (for example `print(1, 2, sep=", ", end="")`).

### Changed

- `print` and `str` now produce CPython's text for every value: real `repr` for lists, tuples, dicts, sets, and bytes; Unicode strings; and shortest-round-trip floats (`1.0` prints as `1.0`, `0.1 + 0.2` as `0.30000000000000004`).
- `/` is now true division and always returns a `float` (`7 / 2` is `3.5`). Use `//` for floor division.
- `==` and `!=` never raise and compare by value across every type, so `x == None` is `False` rather than an error. Ordering (`<`, `<=`, `>`, `>=`) works on numbers, strings, bytes, lists, and tuples, and raises on mismatched types.
- `str + number` is a `TypeError`, matching Python.
- Dict keys can be any hashable value (numbers, `True`/`False`, tuples), and equal keys such as `1`, `1.0`, and `True` collapse to one entry. A missing key (`d[k]`) raises `KeyError`.
- `list.append`, `set.add`, and `dict.get` return `None` (previously `0`), and `dict.get` takes an optional default.
- `for` loops, iterators, and containers all reflect live mutation as in Python; changing a dict or set's size while iterating raises `RuntimeError`.
- New runtime error kinds: `Key error`, `Attribute error`, `Runtime error`, `Value error`, `Lookup error`, `Unicode error`, and `StopIteration`.

### Removed

- The `append(list, value)` built-in extension. Use the `list.append(value)` method.
- The `list(*items)` behavior that built a list from its arguments. `list(iterable)` now converts an iterable, as in Python.

### Compatibility

{: .important }
The instruction set changed. Recompile any standalone `.cvm` modules. Executables packed with an earlier version keep working because each carries its own runtime. The CVM2 container format is unchanged at version 3.

## v0.2.1
{: .d-inline-block }

Released
{: .label }

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
