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

cVM uses Python's own parser, so programs are written in normal Python syntax. Only a defined subset is compiled. Anything else produces a compiler error rather than being silently misinterpreted.

{: .note }
This page describes what you can write. The [Compatibility](COMPATIBILITY.md) page lists every feature's status and where behavior differs from Python.

## Values

| Type | Example |
|:-----|:--------|
| `None` | `None` |
| Integers (signed 64-bit) | `42`, `-7` |
| Floats | `3.14` |
| Strings | `"cVM"` |
| Bytes | `b"abc"` |
| Lists | `[1, 2, 3]` |
| Tuples | `(1, 2)` |
| Dictionaries | `{"a": 1}` |
| Sets | `{1, 2}` |
| Functions | `print`, `add` |

`True` and `False` are stored as the integers `1` and `0`.

## Variables

```python
x = 10
name = "cVM"
items[0] = x
```

Module-level assignments create globals. Assignments inside a function create locals. Only one assignment target is allowed per statement.

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

Arguments are bound in this order:

1. Positional arguments fill the leading parameters.
2. Keyword arguments fill parameters by name.
3. Remaining parameters take their default values.

Too many arguments, an unknown keyword, the same parameter given twice, or a parameter left without a value is a runtime type error.

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
- Keyword arguments to built-in functions and methods
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
```

- `for` iterates lists, tuples, strings, bytes, and dictionary keys.
- `range()` takes one to three integer constants with a non-negative step.
- `break`, `continue`, and `pass` are supported.
- `for ... else` and `while ... else` are not supported.

## Operators

| Kind | Operators |
|:-----|:----------|
| Arithmetic | `+` `-` `*` `/` `//` `%` `**` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Membership | `in` `not in` |
| Boolean | `and` `or` `not` |
| Unary | `+` `-` `~` |
| Augmented assignment | `+=` `-=` `*=` `/=` `//=` `%=` `**=` |

- `/` between two integers performs truncating integer division.
- `//` and `%` follow Python's sign rules.
- Comparisons work on numbers, strings, and bytes, and `==` / `!=` also work on tuples. Comparing other values, including `None`, is a runtime type error.
- Chained comparisons such as `a < b < c` are not supported.

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
```

## Comprehensions

```python
[x * 2 for x in values]
{x * 2 for x in values}
{x: x * 2 for x in values}
```

Comprehensions take a single `for` clause without an `if` clause, and can be nested.

## Built-ins and methods

| Built-in | Purpose |
|:---------|:--------|
| `print(*values)` | Print values separated by spaces |
| `len(x)` | Length of a string, bytes, list, tuple, dict, or set |
| `int(x)` | Convert to an integer |
| `float(x)` | Convert to a float |
| `str(x)` | Convert to a string |
| `list(*items)` | Build a list from the arguments |
| `enumerate(items)` | List of `[index, item]` pairs |
| `append(items, value)` | Append to a list |

| Method | Purpose |
|:-------|:--------|
| `list.append(value)` | Append to a list |
| `dict.get(key)` | Look up a key |
| `set.add(value)` | Add to a set |

`len()` compiles to a single instruction unless `len` is a parameter or local variable, or the module defines its own `def len`.

## Output

`print` separates its arguments with spaces. Numbers, strings, and `None` print as you would expect.

{: .note }
Lists, tuples, dicts, sets, and bytes print as summaries such as `[list len=3]`, and floats print with up to 6 significant digits. See [Printing](COMPATIBILITY.md#printing-and-string-conversion).

## Errors

The compiler reports unsupported syntax and invalid constructs as `compile error: ...`.

At runtime, the VM stops with `VM run error: <kind>` for type errors, argument binding errors, division by zero, integer overflow, stack errors, and invalid bytecode. There is no exception handling.
