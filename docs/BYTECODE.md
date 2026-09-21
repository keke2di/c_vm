---
title: Bytecode
nav_order: 5
description: "The cVM instruction set, stack model, and calling convention."
---

# Bytecode
{: .no_toc }

The instruction set executed by the native VM, and how calls work.
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

The compiler turns every function into a stream of instructions. Each instruction is a 1-byte opcode, optionally followed by a 4-byte little-endian unsigned operand.

Opcode values are defined in `compiler/opcodes.py`. The C header `vm_c/opcodes.h` is generated from that file.

{: .important }
Opcodes and their semantics are compatibility-sensitive. Changing them requires a new [CVM2 format version](CVM2_FORMAT.md#compatibility).

Use `python debug_disasm.py <file.py>` to print the instructions the compiler emits for a program.

## Stack model

Most instructions take their inputs from the operand stack and push their result back onto it:

```text
before:    ...  left  right
BINARY_ADD
after:     ...  result
```

Values are reference-counted by the native runtime. In the tables below, the stack column shows the values an instruction consumes and produces, with the top of the stack on the right.

## Instruction reference

### Constants, variables, and stack

| Opcode | Hex | Operand | Stack | Description |
|:-------|:----|:--------|:------|:------------|
| `NOP` | `0x00` | | | Does nothing |
| `LOAD_CONST` | `0x01` | constant index | → value | Push a constant |
| `LOAD_FAST` | `0x02` | local slot | → value | Push a local variable |
| `STORE_FAST` | `0x03` | local slot | value → | Store a local variable |
| `LOAD_GLOBAL` | `0x04` | name index | → value | Push a global; fails with `Function not found` if it is unset |
| `STORE_GLOBAL` | `0x05` | name index | value → | Store a global |
| `POP_TOP` | `0x06` | | value → | Discard the top value |
| `DUP_TOP` | `0x07` | | a → a a | Duplicate the top value |
| `DUP_TOP_TWO` | `0x0D` | | a b → a b a b | Duplicate the top two values |
| `ROT_TWO` | `0x08` | | a b → b a | Swap the top two values |
| `ROT_THREE` | `0x09` | | a b c → c a b | Rotate the top three values |
| `DELETE_FAST` | `0x0A` | local slot | | `del` a local |
| `DELETE_GLOBAL` | `0x0B` | name index | | `del` a global |
| `DELETE_SUBSCR` | `0x0C` | | container index → | `del container[index]` |

### Arithmetic

| Opcode | Hex | Stack | Description |
|:-------|:----|:------|:------------|
| `BINARY_ADD` | `0x10` | a b → result | `a + b`; also joins strings |
| `BINARY_SUB` | `0x11` | a b → result | `a - b` |
| `BINARY_MUL` | `0x12` | a b → result | `a * b` |
| `BINARY_DIV` | `0x13` | a b → result | `a / b`; true division, always a float |
| `BINARY_MOD` | `0x14` | a b → result | `a % b`, with Python sign rules |
| `BINARY_POW` | `0x15` | a b → result | `a ** b` |
| `BINARY_FLOORDIV` | `0x16` | a b → result | `a // b` |
| `UNARY_NEG` | `0x17` | a → result | `-a` |
| `UNARY_NOT` | `0x18` | a → result | `not a`, producing `True` or `False` |
| `UNARY_POS` | `0x19` | a → result | `+a` |
| `UNARY_INVERT` | `0x1A` | a → result | `~a`, integers only |
| `BINARY_AND` | `0x1B` | a b → result | `a & b`, integers only |
| `BINARY_OR` | `0x1C` | a b → result | <code>a &#124; b</code>, integers only |
| `BINARY_XOR` | `0x1D` | a b → result | `a ^ b`, integers only |
| `BINARY_LSHIFT` | `0x1E` | a b → result | `a << b`; rejects negative counts and detects overflow |
| `BINARY_RSHIFT` | `0x1F` | a b → result | `a >> b`; rejects negative counts |

### Comparison and membership

| Opcode | Hex | Stack | Description |
|:-------|:----|:------|:------------|
| `COMPARE_EQ` | `0x20` | a b → bool | `a == b`; by value, never raises |
| `COMPARE_NE` | `0x21` | a b → bool | `a != b`; by value, never raises |
| `COMPARE_LT` | `0x22` | a b → bool | `a < b` |
| `COMPARE_LE` | `0x23` | a b → bool | `a <= b` |
| `COMPARE_GT` | `0x24` | a b → bool | `a > b` |
| `COMPARE_GE` | `0x25` | a b → bool | `a >= b` |
| `CONTAINS` | `0x26` | item container → bool | `item in container` |
| `COMPARE_IS` | `0x27` | a b → bool | `a is b` |
| `COMPARE_IS_NOT` | `0x28` | a b → bool | `a is not b` |

### Control flow

| Opcode | Hex | Operand | Stack | Description |
|:-------|:----|:--------|:------|:------------|
| `JUMP` | `0x30` | target | | Jump to `target` |
| `JUMP_IF_FALSE` | `0x31` | target | condition → | Jump if the condition is falsy |
| `JUMP_IF_TRUE` | `0x32` | target | condition → | Jump if the condition is truthy |

Jump targets are byte offsets from the start of the current function's code. The compiler emits jumps against labels and resolves them when a function is finalized.

### Calls

| Opcode | Hex | Operand | Stack | Description |
|:-------|:----|:--------|:------|:------------|
| `CALL` | `0x40` | argument count | callable args → result | Call with positional arguments |
| `RETURN` | `0x41` | | value → | Return from the current function |
| `CALL_KW` | `0x42` | argument count | callable args names → result | Call with keyword arguments |
| `CALL_METHOD` | `0x62` | argument count | object name args → result | Call a method; `name` is a string constant |
| `CALL_METHOD_KW` | `0x65` | argument count | object name args kwnames → result | Call a method with keyword arguments |

See [Calls](#calls-in-detail) for the argument layout and binding rules.

### Collections

| Opcode | Hex | Operand | Stack | Description |
|:-------|:----|:--------|:------|:------------|
| `BUILD_LIST` | `0x50` | item count | items → list | Build a list |
| `BUILD_TUPLE` | `0x51` | item count | items → tuple | Build a tuple |
| `BUILD_MAP` | `0x52` | pair count | key value ... → dict | Build a dict from key/value pairs |
| `GET_INDEX` | `0x53` | | container index → item | `container[index]` |
| `SET_INDEX` | `0x54` | | container index value → | `container[index] = value` |
| `LIST_APPEND` | `0x55` | | list value → | Append to a list |
| `LEN` | `0x56` | | value → length | `len(value)` |
| `BUILD_SET` | `0x57` | item count | items → set | Build a set |
| `SET_ADD` | `0x58` | | set value → | Add to a set |
| `MAP_ADD` | `0x59` | | dict key value → | Set a dict entry |
| `GET_SLICE` | `0x5A` | | container start stop step → slice | `container[start:stop:step]`; omitted parts are `None` |
| `STORE_SLICE` | `0x66` | | value container start stop step → | `container[start:stop:step] = value`; the value is on the stack below the container |
| `DELETE_SLICE` | `0x67` | | container start stop step → | `del container[start:stop:step]` |
| `LOAD_DEREF` | `0x68` | cell index | → value | Read a cell or free variable |
| `STORE_DEREF` | `0x69` | cell index | value → | Write a cell or free variable |
| `DELETE_DEREF` | `0x6A` | cell index | → | Empty a cell, so the next read raises |
| `LOAD_CLOSURE` | `0x6B` | cell index | → cell | Push the cell itself, to build a closure |
| `MAKE_FUNCTION` | `0x6C` | function index | defaults kwdefaults closure → function | Build a function value; `defaults` is a tuple, `kwdefaults` a dict, `closure` a tuple of cells whose length must equal the record's free-variable count |
| `GET_ITER_ITEM` | `0x5B` | | container index → item | The item at position `index`; for dicts, the key at that position |

### Iteration and strings

| Opcode | Hex | Operand | Stack | Description |
|:-------|:----|:--------|:------|:------------|
| `GET_ITER` | `0x5C` | | iterable → iterator | Make an iterator |
| `FOR_ITER` | `0x5D` | target | iterator → iterator item | Push the next item, or jump to `target` when exhausted |
| `UNPACK_SEQUENCE` | `0x5E` | count | iterable → itemN ... item1 | Unpack exactly `count` items |
| `UNPACK_EX` | `0x5F` | counts | iterable → ... | Unpack with one starred target; operand packs the before/after counts |
| `FORMAT_VALUE` | `0x63` | conversion | value spec → text | Format one f-string field |
| `BUILD_STRING` | `0x64` | piece count | pieces → string | Concatenate f-string pieces |
| `CALL_EX` | `0x6D` | | callable args kwargs → result | Call with unpacked arguments; `args` is a list or tuple, `kwargs` a dict or `None` |
| `LIST_EXTEND` | `0x6E` | | list iterable → | Extend a list from any iterable |
| `DICT_MERGE` | `0x6F` | | dict other → | Merge a dict, rejecting a key that already exists (this is how `**` in a call detects duplicates) |
| `SETUP_HANDLER` | `0x70` | target | → | Push a handler: the operand is the code offset to jump to when an exception reaches this frame, together with the current stack depth |
| `POP_HANDLER` | `0x71` | | → | Pop the innermost handler (leaving the protected region normally) |
| `EXCEPT_MATCH` | `0x72` | | exception types → exception bool | Test an exception against a type or tuple of types, leaving the exception on the stack |
| `EXCEPT_CLEAR` | `0x73` | | exception → | Drop the handled exception and clear the VM's current-exception state |
| `RERAISE` | `0x74` | | [exception] → | Re-raise the exception on the stack, or the active one |
| `RAISE_VARARGS` | `0x75` | 0 or 1 | [value] → | `0` re-raises the active exception; `1` raises the value on the stack (an instance, or a class that is instantiated with no arguments) |

### Reserved opcodes

`PRINT` (`0x60`), `CALL_BUILTIN` (`0x61`), and `HALT` (`0xFF`) are listed in the opcode table but are not emitted by the compiler. The VM rejects them as invalid opcodes.

### Compiler aliases

The compiler emits some instructions under alternative names:

| Alias | Opcode |
|:------|:-------|
| `CALL_FUNCTION` | `CALL` |
| `RETURN_VALUE` | `RETURN` |
| `POP_JUMP_IF_FALSE` | `JUMP_IF_FALSE` |
| `POP_JUMP_IF_TRUE` | `JUMP_IF_TRUE` |
| `JUMP_ABSOLUTE` | `JUMP` |

## Functions

Every compiled function has metadata describing:

- Function name
- Bytecode offset
- Local-variable count
- Parameter count and parameter names
- Default values for trailing parameters, as constant-pool references

When a module starts, the VM stores a function value in the global slot of every user-defined function and of every built-in function whose name the module uses. User-defined functions replace built-ins with the same name.

A call to a user-defined function creates a native VM frame. The frame tracks the return instruction pointer, locals, function metadata, and stack base.

## Calls in detail

`CALL n` calls a function value with `n` positional arguments:

```text
before:    ...  callable  arg1 ... argN
CALL n
after:     ...  result
```

`CALL_KW n` calls a function value with keyword arguments. `n` counts every argument value. The top of the stack holds a constant tuple of keyword names, matching the last values in order:

```text
before:    ...  callable  pos1 ... posP  kw1 ... kwK  ("name1", ..., "nameK")
CALL_KW P+K
after:     ...  result
```

The callee is evaluated before its arguments. Arguments are bound in this order:

1. Positional values fill the leading parameters.
2. Keyword values fill parameters by name.
3. Remaining parameters take their default values.

These are type errors:

- Calling a value that is not a function
- Passing too many positional arguments
- Passing an unknown or repeated keyword
- Leaving a required parameter without a value

Built-in functions, type constructors, and methods accept keyword arguments too. Method calls with keywords use `CALL_METHOD_KW`, which carries a keyword-name tuple on top of the stack like `CALL_KW`.

## Error handling

The VM checks every instruction as it executes: operands must reference valid constants, locals, globals, and jump targets, and operand types must be supported. Failures stop the program with an error code such as `Type error`, `Bounds error`, `Stack error`, or `Invalid opcode`.

{: .warning }
Bytecode is not trusted input. A malformed module is rejected by the loader or stopped by these runtime checks rather than executed.
