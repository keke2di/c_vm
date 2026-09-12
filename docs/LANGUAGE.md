# cVM Language

cVM implements a defined subset of Python syntax.

It uses Python's AST parser as the front end, but it does not attempt to execute arbitrary Python programs.

Unsupported syntax produces a compiler error.

## Values

The compiler/runtime support values including:

- `None`
- Integers
- Floating-point numbers
- Strings
- Bytes
- Lists
- Tuples
- Dictionaries
- Sets
- Functions

## Variables

Simple assignments are supported:

```python
x = 10
name = "cVM"
```

Module-level assignments are globals. Assignments inside functions use local variables.

## Functions

Function definitions with positional arguments are supported:

```python
def add(a, b):
    return a + b

result = add(2, 3)
```

Functions can return values with `return`.

## Conditionals

Basic conditional statements are supported:

```python
if x > 10:
    print(x)
else:
    print(0)
```

Chained or more advanced Python conditional constructs are outside the supported subset.

## Loops

`while` loops are supported:

```python
while x < 10:
    x = x + 1
```

`for` loops are supported over supported collections:

```python
for value in items:
    print(value)
```

`range()` is supported with one to three integer constant arguments. Negative steps are not supported.

`break` and `continue` are supported inside loops.

## Operators

Supported binary operations include:

```text
+
-
*
/
//
%
**
```

Supported comparisons include:

```text
==
!=
<
<=
>
>=
```

Membership tests:

```text
in
not in
```

Unary operators include:

```text
+
-
~
not
```

Boolean `and` and `or` are supported.

Chained comparisons are not supported.

## Collections

The compiler supports:

```python
[1, 2, 3]
(1, 2, 3)
{1, 2, 3}
{"a": 1, "b": 2}
```

Indexing:

```python
items[0]
mapping["key"]
```

Slicing:

```python
items[1:3]
items[:3]
items[1:]
items[::2]
```

## Comprehensions

Single-generator list, set, and dictionary comprehensions are supported.

Examples:

```python
[x * 2 for x in values]
{x * 2 for x in values}
{x: x * 2 for x in values}
```

Comprehensions with `if` clauses or multiple generators are not currently supported.

## Built-ins and methods

`len()` is supported directly by the compiler.

Simple method calls are supported:

```python
value.method(argument)
```

The available methods depend on the native runtime's supported value operations.

## Assignment limitations

Only a single assignment target is supported.

For example:

```python
x = 1
```

is supported, while multiple-target assignment is outside the current subset.

Assignment to an indexed container is supported:

```python
items[0] = value
```

## Errors

The compiler reports unsupported syntax and invalid constructs as compilation errors.

The native runtime reports execution failures such as type errors, stack errors, division by zero, overflow, invalid references, and malformed bytecode.

## Scope

The language is intentionally small.

The goal of the first public release is a predictable supported subset rather than broad Python compatibility.
