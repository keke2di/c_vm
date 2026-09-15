---
title: Language Reference
nav_order: 3
description: "The Python subset that cVM compiles and runs."
---

# Language Reference
{: .no_toc }

The Python subset that cVM compiles and runs.
{: .fs-6 .fw-300 }

<details open markdown="block">
  <summary>
    On this page
  </summary>
  {: .text-delta }
1. TOC
{:toc}
</details>

## Overview

cVM uses Python's own parser, so programs are written in normal Python syntax. Only a defined subset is compiled. Anything else produces a compiler error rather than being silently misinterpreted. Within the subset, output matches CPython 3.14.

{: .note }
This page describes what you can write. The [Compatibility](COMPATIBILITY.md) page lists every feature's status and where behavior differs from Python.

## Values

| Type | Example |
|:-----|:--------|
| `None` | `None` |
| Integers (signed 64-bit) | `42`, `-7` |
| Floats | `3.14` |
| Strings (Unicode) | `"café"` |
| Bytes | `b"abc"` |
| Lists | `[1, 2, 3]` |
| Tuples | `(1, 2)` |
| Dictionaries | `{"a": 1}` |
| Sets, frozensets | `{1, 2}`, `frozenset({1})` |
| Ranges | `range(0, 10, 2)` |
| Functions | `print`, `add` |

`True` and `False` print as `True`/`False` but are a subtype of `int`: they equal `1` and `0` and behave as integers in arithmetic and as dict keys.

## Variables and assignment

```python
x = 10
name = "cVM"
items[0] = x

a, b = 1, 2
first, *rest = [1, 2, 3]
a = b = 0
```

Module-level assignments create globals. Assignments inside a function create locals. Targets may be a single name, a subscript, or a tuple/list to unpack (with at most one starred target).

`del` removes a name or a subscript:

```python
del x
del d["key"]
```

## Functions

Top-level functions with positional parameters:

```python
def add(a, b):
    return a + b

result = add(2, 3)
```

Functions can be called before their definition appears in the file.

### Keyword arguments and defaults

```python
def greet(name, punctuation="!"):
    return "Hello " + name + punctuation

greet("Ada")
greet("Ada", "?")
greet(punctuation=".", name="Ada")
```

Arguments are bound positional first, then by keyword, then from defaults. Too many arguments, an unknown keyword, a repeated parameter, or a parameter left without a value is a runtime type error. Keyword arguments also work for built-in functions and methods, such as `print(1, 2, sep=", ", end="")`.

{: .important }
Default values must be constant expressions: numbers, strings, bytes, `None`, `True`, `False`, and tuples of these. They are fixed when the program is compiled.

### Functions as values

User-defined and built-in functions are values. They can be assigned, passed, returned, and stored in collections, and any expression that evaluates to a function can be called:

```python
def apply(fn, value):
    return fn(value)

p = print
p(apply(str, 42))

ops = [add]
ops[0](1, 2)
```

Calling a value that is not a function is a runtime type error. A top-level `def` that uses a built-in's name replaces that built-in in the module.

### Not supported

- Nested functions, closures, and `lambda`
- Decorators
- `*args`, `**kwargs`, keyword-only and positional-only parameters
- Argument unpacking with `*` or `**`
- The `global` and `nonlocal` statements

## Control flow

### Conditionals

```python
if x > 10:
    print(x)
elif x > 5:
    print(5)
else:
    print(0)

label = "big" if x > 10 else "small"
```

### Loops

```python
while x < 10:
    x += 1

for row in rows:
    for value in row:
        print(value)

for i in range(0, 10, 2):
    print(i)

for key, value in pairs:
    print(key, value)

for item in items:
    if item == target:
        break
else:
    print("not found")
```

- `for` iterates any iterable: lists, tuples, strings, bytes, dicts, sets, ranges, and the lazy iterators returned by `enumerate`, `zip`, `map`, `filter`, and `reversed`.
- The loop target may be a name or a tuple/list to unpack.
- `range()` takes one to three integer arguments and supports negative steps.
- `break`, `continue`, and `pass` are supported, as are `for ... else` and `while ... else`.

## Operators

| Kind | Operators |
|:-----|:----------|
| Arithmetic | `+` `-` `*` `/` `//` `%` `**` |
| Bitwise | `&` `\|` `^` `<<` `>>` `~` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Membership | `in` `not in` |
| Identity | `is` `is not` |
| Boolean | `and` `or` `not` |
| Unary | `+` `-` `~` |
| Augmented assignment | `+=` `-=` `*=` `/=` `//=` `%=` `**=` `&=` `\|=` `^=` `<<=` `>>=` |

- `/` is true division and always returns a `float`; use `//` for floor division.
- `+` and `*` also concatenate and repeat `list`, `tuple`, `str`, and `bytes`.
- `==` and `!=` compare by value across all types and never raise. Ordering works on numbers, strings, bytes, lists, and tuples; comparing mismatched types raises.
- Chained comparisons such as `a < b < c` are supported.

## Collections

```python
items = [1, 2, 3]
point = (1, 2)
tags = {"a", "b"}
ages = {"ada": 36}

items[0]
items[-1]
ages["ada"]

items[1:3]
items[::2]
"abc"[::-1]

ages["linus"] = 30
del ages["ada"]
"ada" in ages
```

Dict keys may be any hashable value. `dict.keys()`, `dict.values()`, and `dict.items()` return live views.

## Comprehensions

```python
[x * 2 for x in values]
{x * 2 for x in values}
{x: x * 2 for x in values}
[x for row in grid for x in row if x > 0]
```

Comprehensions support multiple `for` clauses, `if` filters, and tuple targets, and can be nested. Generator expressions are not supported; use a list comprehension.

## f-strings

```python
name = "Ada"
score = 91.5
print(f"{name}: {score:.1f}")
print(f"{255:#x} {1000000:,} {name!r}")
```

f-strings support the full format-spec mini-language (fill, alignment, sign, `#`, `0`, width, grouping, precision, and type) and the `!r`, `!s`, and `!a` conversions.

## Built-ins and methods

| Built-in | Purpose |
|:---------|:--------|
| `print`, `len`, `repr`, `ascii`, `format` | Output and text conversion |
| `int`, `float`, `str`, `bool`, `bytes` | Scalar conversions |
| `list`, `tuple`, `set`, `frozenset`, `dict`, `range`, `type` | Containers and types |
| `abs`, `divmod`, `pow`, `round` | Numeric |
| `sum`, `min`, `max`, `sorted`, `any`, `all` | Aggregates over iterables |
| `enumerate`, `zip`, `map`, `filter`, `reversed`, `iter`, `next` | Iteration |
| `isinstance`, `callable`, `id`, `ord`, `chr`, `bin`, `oct`, `hex` | Inspection and conversion |

Every public method of `str`, `bytes`, `list`, `dict`, `set`, and `frozenset` is available:

| Type | Methods |
|:-----|:--------|
| `str`, `bytes` | Searching (`find`, `index`, `count`, `startswith`, `endswith`), editing (`replace`, `strip`, `removeprefix`, `translate`), splitting and joining (`split`, `splitlines`, `join`, `partition`), case (`upper`, `lower`, `title`, `casefold`), padding (`center`, `ljust`, `zfill`), the `is*` predicates, and `format`/`format_map` |
| `list` | `append`, `extend`, `insert`, `pop`, `remove`, `clear`, `index`, `count`, `reverse`, `copy`, `sort` |
| `dict` | `get`, `keys`, `values`, `items`, `pop`, `popitem`, `setdefault`, `update`, `clear`, `copy`, `fromkeys` |
| `set`, `frozenset` | `add`, `remove`, `discard`, `pop`, `update`, `union`, `intersection`, `difference`, `symmetric_difference`, `issubset`, `issuperset`, `isdisjoint`, and more |

```python
print("a,b,c".split(","))
print("-".join(["x", "y"]))
print("Hello {}, you are {age}".format("Ada", age=36))
print({1, 2} | {2, 3}, {1, 2} & {2, 3})
```

The full matrix is in [Compatibility](COMPATIBILITY.md#methods).

`len()` compiles to a single instruction unless `len` is used as a value or the name `len` is bound in the program.

## Output

`print` and `str` produce CPython's text: real `repr` for containers, Unicode strings, and shortest-round-trip floats. See [Printing](COMPATIBILITY.md#printing-and-string-conversion).

## Errors

The compiler reports unsupported syntax and invalid constructs as `compile error: ...`.

At runtime, the VM stops with `VM run error: <kind>` for type errors, value errors, missing keys, out-of-range indexes, division by zero, integer overflow, and other conditions. There is no exception handling. The full list of error kinds is in [Compatibility](COMPATIBILITY.md#runtime-limits-and-errors).
