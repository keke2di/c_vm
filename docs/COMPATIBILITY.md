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

cVM aims for predictable behavior, not full Python compatibility. Constructs outside this matrix are rejected by the compiler or stopped with a runtime error.

| Label | Meaning |
|:------|:--------|
| <span class="label label-green">Supported</span> | Behaves like Python for normal use |
| <span class="label label-yellow">Partial</span> | Works within the limits noted |
| <span class="label label-purple">Differs</span> | Works, but the result is not what Python gives |
| <span class="label label-blue">cVM extension</span> | Not valid Python, but accepted by cVM |
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
| `None` | <span class="label label-green">Supported</span> | |
| `bool` | <span class="label label-yellow">Partial</span> | A subtype of `int`: `True`/`False` print as `True`/`False` but equal `1`/`0` and behave as integers in arithmetic, indexing, and dict keys. Comparisons, membership, and `not` produce `bool`. |
| `int` | <span class="label label-yellow">Partial</span> | Signed 64-bit. Overflow is a runtime error; there are no big integers. |
| `float` | <span class="label label-yellow">Partial</span> | IEEE double precision arithmetic. Printing differs, see [Printing](#printing-and-string-conversion). |
| `str` | <span class="label label-green">Supported</span> | |
| `bytes` | <span class="label label-yellow">Partial</span> | Literals, indexing, slicing, iteration (yields integers), `len`, `in`, comparisons. |
| `list` | <span class="label label-green">Supported</span> | |
| `tuple` | <span class="label label-green">Supported</span> | |
| `dict` | <span class="label label-yellow">Partial</span> | Literals, lookup by key, iteration over keys, `in`, `len`, `get`. Item assignment `d[k] = v` works only with string keys. |
| `set` | <span class="label label-yellow">Partial</span> | Literals, `in`, `len`, `add`. Not iterable in `for` loops or comprehensions. |
| Functions | <span class="label label-green">Supported</span> | User-defined and built-in functions are first-class values. |

## Statements

| Statement | Status | Notes |
|:----------|:-------|:------|
| `x = value` | <span class="label label-green">Supported</span> | Single target only. |
| `a[i] = value` | <span class="label label-yellow">Partial</span> | Lists with integer indexes (negative allowed), dicts with string keys. |
| `a = b = 1`, `a, b = pair`, starred targets | <span class="label label-red">Not supported</span> | |
| `x: int = 1` | <span class="label label-red">Not supported</span> | |
| `x += value` and other augmented assignment | <span class="label label-yellow">Partial</span> | Simple names only. |
| `if` / `elif` / `else` | <span class="label label-green">Supported</span> | |
| `while` | <span class="label label-green">Supported</span> | |
| `for` | <span class="label label-yellow">Partial</span> | The target must be a simple name. Iterates lists, tuples, strings, bytes and dict keys. Loops can be nested. |
| `for i in range(...)` | <span class="label label-yellow">Partial</span> | 1 to 3 integer constant arguments, non-negative step. The loop variable is the counter, so assigning to it inside the body changes the iteration. |
| `for ... else`, `while ... else` | <span class="label label-red">Not supported</span> | |
| `break`, `continue`, `pass`, `return` | <span class="label label-green">Supported</span> | |
| `global`, `nonlocal` | <span class="label label-red">Not supported</span> | |
| `del`, `assert` | <span class="label label-red">Not supported</span> | |
| `try` / `except` / `finally`, `raise` | <span class="label label-red">Not supported</span> | |
| `with` | <span class="label label-red">Not supported</span> | |
| `yield`, `async`, `await` | <span class="label label-red">Not supported</span> | |

## Expressions

| Expression | Status | Notes |
|:-----------|:-------|:------|
| Literals | <span class="label label-green">Supported</span> | Numbers, strings, bytes, `None`, `True`, `False`, lists, tuples, sets, dicts. Complex numbers are not supported. |
| `+ - * // % **` | <span class="label label-yellow">Partial</span> | Numeric operands. `+` also joins strings. `//` and `%` follow Python's sign rules. |
| `/` on two integers | <span class="label label-purple">Differs</span> | Truncating integer division instead of a float: `7 / 2` is `3`, `-7 / 2` is `-3`. With a float operand the result is a float. |
| `str + number` | <span class="label label-purple">Differs</span> | Converts the number to text instead of raising `TypeError`. |
| List or tuple concatenation, sequence repetition | <span class="label label-red">Not supported</span> | |
| `== != < <= > >=` | <span class="label label-yellow">Partial</span> | Numbers, strings and bytes. Tuples with `==` and `!=` only. Comparing other types is a runtime type error, including `x == None`, lists, dicts and functions. |
| Chained comparisons `a < b < c` | <span class="label label-red">Not supported</span> | |
| `is`, `is not` | <span class="label label-green">Supported</span> | Identity comparison; `None`, `True`, `False`, and type objects are singletons. |
| `in`, `not in` | <span class="label label-green">Supported</span> | Lists, tuples, strings, bytes, dict keys, sets. |
| `and`, `or`, `not` | <span class="label label-green">Supported</span> | `and` and `or` return one of their operands, as in Python. |
| Unary `-`, `+`, `~` | <span class="label label-green">Supported</span> | `~` on integers only. |
| Indexing `x[i]` | <span class="label label-green">Supported</span> | Lists, tuples, strings and bytes by integer index, negative indexes included; dicts by key. |
| Slicing `x[a:b:c]` | <span class="label label-green">Supported</span> | Lists, tuples, strings, bytes. Bounds must be integers or omitted. Negative steps work. |
| Calls | <span class="label label-green">Supported</span> | Any expression that evaluates to a function can be called: `f(x)`, `choose()(x)`, `ops[0](x)`. |
| Method calls `obj.method(...)` | <span class="label label-yellow">Partial</span> | See [Methods](#methods). |
| Other attribute access | <span class="label label-red">Not supported</span> | |
| `x if cond else y` | <span class="label label-red">Not supported</span> | |
| f-strings | <span class="label label-red">Not supported</span> | |
| `:=`, starred expressions, `**` in dict literals | <span class="label label-red">Not supported</span> | |

## Comprehensions

| Feature | Status | Notes |
|:--------|:-------|:------|
| List, set, dict comprehensions | <span class="label label-yellow">Partial</span> | One `for` clause with a simple-name target and no `if` clause. Comprehensions can be nested. |
| Comprehension variable scope | <span class="label label-purple">Differs</span> | The loop variable stays defined after the comprehension. |
| Generator expressions | <span class="label label-red">Not supported</span> | |

## Functions

| Feature | Status | Notes |
|:--------|:-------|:------|
| Positional parameters | <span class="label label-green">Supported</span> | Argument count is checked at runtime. |
| Keyword arguments | <span class="label label-green">Supported</span> | `f(1, b=2)`, also through function values. Built-in functions and method calls do not accept keyword arguments. |
| Default parameter values | <span class="label label-yellow">Partial</span> | Defaults must be constant expressions: numbers, strings, bytes, `None`, `True`, `False` and tuples of these. They are fixed at compile time instead of being evaluated when `def` runs. |
| Functions as values | <span class="label label-green">Supported</span> | Assign, pass, return and store functions, including built-ins: `p = print`. |
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

| Built-in | Status | Notes |
|:---------|:-------|:------|
| `print(*values)` | <span class="label label-yellow">Partial</span> | Values are separated by spaces. Returns `None`. `sep`, `end` and `file` are not supported. See [Printing](#printing-and-string-conversion). |
| `str(x)` | <span class="label label-purple">Differs</span> | Uses the same text as `print`. See [Printing](#printing-and-string-conversion). |
| `len(x)` | <span class="label label-green">Supported</span> | str, bytes, list, tuple, dict, set. Compiled to an instruction unless `len` is a parameter or local of the current function, or the module has a top-level `def len`. It cannot be used as a value, and `len = ...` at module level does not replace it. |
| `int(x)` | <span class="label label-yellow">Partial</span> | From an int, a float (truncates toward zero) or a base-10 string. |
| `float(x)` | <span class="label label-yellow">Partial</span> | From a float, an int or a string. |
| `list(*items)` | <span class="label label-purple">Differs</span> | Builds a list from its arguments: `list(1, 2)` gives `[1, 2]`. It does not convert an iterable. |
| `enumerate(items)` | <span class="label label-purple">Differs</span> | Lists only. Returns a list of `[index, item]` lists. |
| `append(items, value)` | <span class="label label-blue">cVM extension</span> | Same as `items.append(value)`. |
| `range(...)` | <span class="label label-yellow">Partial</span> | Only directly as the iterable of a `for` statement. |
| `type(x)` | <span class="label label-green">Supported</span> | Returns the object's type; `type(1) is int`. The built-in type names are type objects. |
| Other built-ins (`abs`, `min`, `max`, `sum`, `zip`, `sorted`, `isinstance`, `input`, `open`, ...) | <span class="label label-red">Not supported</span> | |

## Methods

| Method | Status | Notes |
|:-------|:-------|:------|
| `list.append(value)` | <span class="label label-purple">Differs</span> | Returns `0` instead of `None`. |
| `dict.get(key)` | <span class="label label-purple">Differs</span> | One argument only. Returns `0` when the key is missing. |
| `set.add(value)` | <span class="label label-purple">Differs</span> | Returns `0` instead of `None`. |
| Other methods (`str.split`, `list.pop`, `dict.items`, ...) | <span class="label label-red">Not supported</span> | Calling them is a runtime type error. |

## Printing and string conversion

`print` and `str` produce the following text:

| Value | Text |
|:------|:-----|
| `int` | `42` |
| `bool` | `True` or `False` |
| `float` | C `%g` format with up to 6 significant digits: `1.0` prints `1`, `2.5` prints `2.5`, `1e20` prints `1e+20`, `0.1 + 0.2` prints `0.3` |
| `str` | The text itself |
| `None` | `None` |
| `list` | A summary such as `[list len=3]`, not the items |
| `tuple` | `(tuple len=3)` |
| `dict` | `{dict len=3}` |
| `set` | `{set len=3}` |
| `bytes` | `[bytes len=3]` |
| Function | `<function add>` or `<built-in function print>` |

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
| `Division by zero` | `/`, `//` or `%` by zero |
| `Integer overflow` | 64-bit integer overflow |
| `Stack error` | Call depth or value stack exhausted |
| `Bounds error` | Index out of range, invalid bytecode references |
| `Function not found` | Calling or reading an undefined name |
