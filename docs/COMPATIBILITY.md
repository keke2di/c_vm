---
title: Compatibility
nav_order: 4
description: "Support matrix for the Python subset cVM implements, and known differences from Python."
---

# Compatibility
{: .no_toc }

What cVM supports, and where it behaves differently from Python.
{: .fs-6 .fw-300 }

<details open markdown="block">
  <summary>
    On this page
  </summary>
  {: .text-delta }
1. TOC
{:toc}
</details>

## Status labels

cVM aims for predictable behavior and, within the supported subset, output that matches CPython 3.14. Constructs outside this matrix are rejected by the compiler or stopped with a runtime error.

| Label | Meaning |
|:------|:--------|
| <span class="label label-green">Supported</span> | Behaves like Python for normal use |
| <span class="label label-yellow">Partial</span> | Works within the limits noted |
| <span class="label label-purple">Differs</span> | Works, but the result is not what Python gives |
| <span class="label label-red">Not supported</span> | Rejected by the compiler, or a runtime error |

## Program structure

| Feature | Status | Notes |
|:--------|:-------|:------|
| Module-level statements | <span class="label label-green">Supported</span> | Compiled into an entry function. |
| Module-level `return` | <span class="label label-red">Not supported</span> | A `return` outside a function is a compile error. Use `print()` for output. |
| Top-level `def` | <span class="label label-green">Supported</span> | Functions are hoisted and can be called before their definition in the file. |
| Nested `def`, closures | <span class="label label-red">Not supported</span> | |
| `lambda` | <span class="label label-red">Not supported</span> | |
| Decorators | <span class="label label-red">Not supported</span> | |
| `import`, modules | <span class="label label-red">Not supported</span> | |
| `class` | <span class="label label-red">Not supported</span> | |

## Values and types

| Type | Status | Notes |
|:-----|:-------|:------|
| `None` | <span class="label label-green">Supported</span> | A single shared object; `is` works. |
| `bool` | <span class="label label-green">Supported</span> | A subtype of `int`: `True`/`False` print as themselves but equal `1`/`0` and behave as integers in arithmetic, indexing, and dict keys. |
| `int` | <span class="label label-yellow">Partial</span> | Signed 64-bit. Overflow is a runtime error; there are no big integers. |
| `float` | <span class="label label-green">Supported</span> | IEEE double precision. Prints with the shortest round-trip form, as in Python. |
| `str` | <span class="label label-green">Supported</span> | Full Unicode: `len`, indexing, slicing, and iteration are by code point. |
| `bytes` | <span class="label label-green">Supported</span> | Literals, indexing, slicing, iteration (yields integers), `len`, `in`, comparisons, `.decode()`. |
| `list` | <span class="label label-green">Supported</span> | |
| `tuple` | <span class="label label-green">Supported</span> | |
| `dict` | <span class="label label-green">Supported</span> | Any hashable key. Lookup, `d[k] = v`, `del`, `in`, `len`, iteration, `.get()`, `.keys()`, `.values()`, `.items()`. |
| `set` | <span class="label label-green">Supported</span> | Literals, `in`, `len`, iteration, `.add()`. Iteration order is insertion order, not Python's hash order. |
| `frozenset` | <span class="label label-green">Supported</span> | Hashable; usable as a dict key or set element. |
| `range` | <span class="label label-green">Supported</span> | A lazy object: indexing, `len`, `in`, `==`, iteration, negative steps. |
| Dict views | <span class="label label-green">Supported</span> | `dict.keys()`, `.values()`, `.items()` are live views. |
| Functions | <span class="label label-green">Supported</span> | User-defined and built-in functions are first-class values. |
| `complex` | <span class="label label-red">Not supported</span> | |

## Statements

| Statement | Status | Notes |
|:----------|:-------|:------|
| `x = value` | <span class="label label-green">Supported</span> | |
| `a, b = ...`, `a = b = c`, `a, *rest = ...` | <span class="label label-green">Supported</span> | Tuple/list targets, nesting, and one starred target. |
| `a[i] = value` | <span class="label label-green">Supported</span> | Lists by integer index (negative allowed); dicts by any hashable key. |
| `x: int = 1` | <span class="label label-red">Not supported</span> | Variable annotations are rejected. |
| `x += value` and other augmented assignment | <span class="label label-green">Supported</span> | Names and subscripts (`x[i] += 1`). |
| `if` / `elif` / `else` | <span class="label label-green">Supported</span> | |
| `while`, `while ... else` | <span class="label label-green">Supported</span> | |
| `for`, `for ... else` | <span class="label label-green">Supported</span> | Iterates any iterable; the target may be a name or a tuple/list to unpack. |
| `break`, `continue`, `pass`, `return` | <span class="label label-green">Supported</span> | |
| `del` | <span class="label label-green">Supported</span> | Names and subscripts (`del x`, `del d[k]`). Slice deletion is not supported. |
| `global`, `nonlocal` | <span class="label label-red">Not supported</span> | |
| `assert` | <span class="label label-red">Not supported</span> | |
| `try` / `except` / `finally`, `raise` | <span class="label label-red">Not supported</span> | A runtime error stops the program. |
| `with` | <span class="label label-red">Not supported</span> | |
| `yield`, `async`, `await` | <span class="label label-red">Not supported</span> | |

## Expressions

| Expression | Status | Notes |
|:-----------|:-------|:------|
| Literals | <span class="label label-green">Supported</span> | Numbers, strings, bytes, `None`, `True`, `False`, lists, tuples, sets, dicts. Complex numbers are not supported. |
| `+ - * / // % **` | <span class="label label-green">Supported</span> | `+`/`*` also concatenate and repeat sequences. `//` and `%` follow Python's sign rules. |
| `/` | <span class="label label-green">Supported</span> | True division; always returns a `float` (`7 / 2` is `3.5`). |
| `& \| ^ << >> ~` | <span class="label label-green">Supported</span> | Integers only. Shifts reject negative counts and detect 64-bit overflow. |
| `== != < <= > >=` | <span class="label label-green">Supported</span> | `==`/`!=` compare by value across all types and never raise (`x == None` is `False`). Ordering works on numbers, strings, bytes, lists, and tuples; mismatched types raise. |
| Chained comparisons `a < b < c` | <span class="label label-green">Supported</span> | Single evaluation and short-circuiting, as in Python. |
| `is`, `is not` | <span class="label label-green">Supported</span> | Identity comparison. |
| `in`, `not in` | <span class="label label-green">Supported</span> | Lists, tuples, strings, bytes, dicts, sets, ranges, dict views. |
| `and`, `or`, `not` | <span class="label label-green">Supported</span> | `and`/`or` return one of their operands. |
| Unary `-`, `+`, `~` | <span class="label label-green">Supported</span> | `~` on integers only. |
| Indexing `x[i]` | <span class="label label-green">Supported</span> | Sequences by integer index (negatives included); dicts by key; ranges. |
| Slicing `x[a:b:c]` | <span class="label label-green">Supported</span> | Lists, tuples, strings, bytes. Negative steps work. Slice assignment is not supported. |
| Calls | <span class="label label-green">Supported</span> | Any expression that evaluates to a function: `f(x)`, `choose()(x)`, `ops[0](x)`. |
| Method calls `obj.method(...)` | <span class="label label-yellow">Partial</span> | See [Methods](#methods). |
| Other attribute access | <span class="label label-red">Not supported</span> | |
| `x if cond else y` | <span class="label label-green">Supported</span> | |
| f-strings | <span class="label label-green">Supported</span> | Full format-spec mini-language and `!r`/`!s`/`!a` conversions. |
| `:=`, `*` in calls, `**` in dict literals | <span class="label label-red">Not supported</span> | |
| Generator expressions | <span class="label label-red">Not supported</span> | Use a list comprehension. |

## Comprehensions

| Feature | Status | Notes |
|:--------|:-------|:------|
| List, set, dict comprehensions | <span class="label label-green">Supported</span> | Multiple `for` clauses, `if` filters, and tuple targets. Comprehensions can be nested. |
| Comprehension variable scope | <span class="label label-purple">Differs</span> | The loop variable stays defined after the comprehension. |
| Generator expressions | <span class="label label-red">Not supported</span> | |

## Functions

| Feature | Status | Notes |
|:--------|:-------|:------|
| Positional parameters | <span class="label label-green">Supported</span> | Argument count is checked at runtime. |
| Keyword arguments | <span class="label label-green">Supported</span> | For user functions, built-in functions, type constructors, and methods. |
| Default parameter values | <span class="label label-yellow">Partial</span> | Defaults must be constant expressions: numbers, strings, bytes, `None`, `True`, `False`, and tuples of these. They are fixed at compile time. |
| Functions as values | <span class="label label-green">Supported</span> | Assign, pass, return, and store functions, including built-ins: `p = print`. |
| Shadowing built-ins | <span class="label label-green">Supported</span> | A top-level `def str(...)` replaces the built-in `str`. |
| Parameter and return annotations | <span class="label label-yellow">Partial</span> | Accepted and ignored. Type parameters (`def f[T]()`) are not supported. |
| `*args`, `**kwargs`, keyword-only and positional-only parameters | <span class="label label-red">Not supported</span> | |
| Argument unpacking `f(*xs)`, `f(**d)` | <span class="label label-red">Not supported</span> | |
| Recursion | <span class="label label-green">Supported</span> | Up to 255 nested calls. |

## Scope

| Feature | Status | Notes |
|:--------|:-------|:------|
| Module-level assignments create globals | <span class="label label-green">Supported</span> | |
| Assignments inside a function create locals | <span class="label label-green">Supported</span> | |
| Reading globals inside a function | <span class="label label-green">Supported</span> | |
| Assigning globals inside a function | <span class="label label-red">Not supported</span> | There is no `global` statement. |
| `x += 1` on a global inside a function | <span class="label label-purple">Differs</span> | If the function has no local `x`, the global `x` is updated instead of raising `UnboundLocalError`. |
| Reading a name before the function assigns it | <span class="label label-purple">Differs</span> | The global with that name is read instead of raising `UnboundLocalError`. |
| Module-level `for` targets and comprehension variables | <span class="label label-purple">Differs</span> | They are locals of the module body: usable at module level, but not visible inside functions. |

## Built-in functions

Every built-in below matches CPython's result for the supported argument forms, including keyword arguments.

| Built-in | Status | Notes |
|:---------|:-------|:------|
| `print(*values, sep, end, flush)` | <span class="label label-yellow">Partial</span> | `sep`, `end`, and `flush` work. `file` accepts only `None`. |
| `len`, `repr`, `ascii`, `format` | <span class="label label-green">Supported</span> | |
| `abs`, `divmod`, `pow`, `round` | <span class="label label-green">Supported</span> | `pow` includes 3-argument modular form. Results outside 64-bit `int` overflow. |
| `sum`, `min`, `max`, `sorted` | <span class="label label-green">Supported</span> | `min`/`max`/`sorted` take `key=`; `min`/`max` take `default=`; `sorted` takes `reverse=` and is stable. |
| `any`, `all` | <span class="label label-green">Supported</span> | |
| `enumerate`, `zip`, `map`, `filter`, `reversed` | <span class="label label-green">Supported</span> | Return lazy iterators. `zip`/`map` accept `strict=`. |
| `iter`, `next` | <span class="label label-green">Supported</span> | `iter(callable, sentinel)` and `next(it, default)` are supported. |
| `isinstance`, `callable`, `id` | <span class="label label-green">Supported</span> | `isinstance` takes a type or a tuple of types. |
| `ord`, `chr` | <span class="label label-green">Supported</span> | |
| `bin`, `oct`, `hex` | <span class="label label-green">Supported</span> | |
| `int`, `float`, `str`, `bool` | <span class="label label-green">Supported</span> | `int(x, base)` and `float`/`int` string parsing match CPython, including underscores and `0x`/`0o`/`0b` prefixes. `str(bytes, encoding)` decodes. |
| `list`, `tuple`, `set`, `frozenset`, `dict`, `bytes`, `range`, `type` | <span class="label label-green">Supported</span> | The container constructors build from any iterable. |
| `hash`, `input`, `open`, `sorted`… others | <span class="label label-red">Not supported</span> | `hash`, `input`, `open`, `vars`, `getattr`, `super`, etc. are not available. |

## Methods

More methods arrive in later releases; the ones below are available now.

| Method | Status | Notes |
|:-------|:-------|:------|
| `list.append(value)` | <span class="label label-green">Supported</span> | Returns `None`. |
| `dict.get(key[, default])` | <span class="label label-green">Supported</span> | Returns `None` (or the default) when the key is missing. |
| `dict.keys()`, `dict.values()`, `dict.items()` | <span class="label label-green">Supported</span> | Live view objects. |
| `set.add(value)` | <span class="label label-green">Supported</span> | Returns `None`. |
| `str.encode([encoding[, errors]])` | <span class="label label-green">Supported</span> | `utf-8`, `utf-8-sig`, `ascii`, `latin-1`. |
| `bytes.decode([encoding[, errors]])` | <span class="label label-green">Supported</span> | Same encodings. |
| Other methods (`str.split`, `list.pop`, `dict.update`, `set.union`, ...) | <span class="label label-red">Not supported</span> | Calling them is a runtime error. |

## Printing and string conversion

`print` and `str` produce Python's `str()` text, and `repr` produces Python's `repr()`:

| Value | `str` / `print` | `repr` |
|:------|:-----|:-----|
| `int` | `42` | `42` |
| `bool` | `True` | `True` |
| `float` | `1.0`, `2.5`, `0.30000000000000004`, `1e+20` (shortest round-trip) | same |
| `str` | the text itself | `'text'` with quotes and escapes |
| `None` | `None` | `None` |
| `list` | `[1, 2, 3]` | `[1, 2, 3]` |
| `tuple` | `(1, 2)` | `(1, 2)` |
| `dict` | `{'a': 1}` | `{'a': 1}` |
| `set` | `{1, 2}` | `{1, 2}` |
| `bytes` | `b'abc'` | `b'abc'` |
| `range` | `range(0, 5)` | `range(0, 5)` |
| Function | `<function add>` or `<built-in function print>` | same |

{: .note }
`repr` escapes non-printable ASCII; `ascii()` also escapes every non-ASCII character (`ascii("café")` is `'caf\xe9'`). Set and `dict`-view iteration follow insertion order, which can differ from CPython's hash order. Iterator objects print as a generic `<iterator object>`.

## Runtime limits and errors

| Limit | Value |
|:------|:------|
| Integer range | Signed 64-bit |
| Call depth | 256 frames: the module body plus 255 nested calls |
| Value stack | 1024 entries |
| Constants per module | 65536 |
| Names per module | 65535 |
| Functions per module | 4096 |

There is no exception handling. A runtime error stops the program, prints `VM run error: <kind>` to stderr, and exits with a non-zero status.

| Error kind | Typical cause |
|:-----------|:--------------|
| `Type error` | Unsupported operand types, calling a non-function, argument binding errors |
| `Value error` | Bad conversion (`int("x")`), `chr` out of range, wrong unpack length |
| `Key error` | Missing dict key |
| `Bounds error` | Sequence index out of range, invalid bytecode references |
| `Attribute error` | Unknown method |
| `Division by zero` | `/`, `//`, `%`, or `divmod` by zero |
| `Integer overflow` | 64-bit integer overflow |
| `Runtime error` | Dict or set changed size during iteration |
| `Lookup error` | Unknown text encoding or error-handler name |
| `Unicode error` | Encode/decode failure with the `strict` handler |
| `StopIteration` | `next()` past the end with no default |
| `Stack error` | Call depth or value stack exhausted |
| `Function not found` | Reading or calling an undefined name |
