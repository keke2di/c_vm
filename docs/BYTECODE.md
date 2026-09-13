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

### Arithmetic

| Opcode | Hex | Stack | Description |
|:-------|:----|:------|:------------|
| `BINARY_ADD` | `0x10` | a b → result | `a + b`; also joins strings |
| `BINARY_SUB` | `0x11` | a b → result | `a - b` |
| `BINARY_MUL` | `0x12` | a b → result | `a * b` |
| `BINARY_DIV` | `0x13` | a b → result | `a / b`; integer division when both are integers |
| `BINARY_MOD` | `0x14` | a b → result | `a % b`, with Python sign rules |
| `BINARY_POW` | `0x15` | a b → result | `a ** b` |
| `BINARY_FLOORDIV` | `0x16` | a b → result | `a // b` |
| `UNARY_NEG` | `0x17` | a → result | `-a` |
| `UNARY_NOT` | `0x18` | a → result | `not a`, producing `1` or `0` |
| `UNARY_POS` | `0x19` | a → result | `+a` |
| `UNARY_INVERT` | `0x1A` | a → result | `~a`, integers only |

### Comparison and membership

| Opcode | Hex | Stack | Description |
|:-------|:----|:------|:------------|
| `COMPARE_EQ` | `0x20` | a b → 1 or 0 | `a == b` |
| `COMPARE_NE` | `0x21` | a b → 1 or 0 | `a != b` |
| `COMPARE_LT` | `0x22` | a b → 1 or 0 | `a < b` |
| `COMPARE_LE` | `0x23` | a b → 1 or 0 | `a <= b` |
| `COMPARE_GT` | `0x24` | a b → 1 or 0 | `a > b` |
| `COMPARE_GE` | `0x25` | a b → 1 or 0 | `a >= b` |
| `CONTAINS` | `0x26` | item container → 1 or 0 | `item in container` |

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
| `GET_ITER_ITEM` | `0x5B` | | container index → item | The item at position `index`; for dicts, the key at that position |

### Reserved opcodes

`ROT_TWO` (`0x08`), `PRINT` (`0x60`), `CALL_BUILTIN` (`0x61`), and `HALT` (`0xFF`) are listed in the opcode table but are not emitted by the compiler. The VM rejects them as invalid opcodes.

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
- Passing keyword arguments to a built-in function

## Error handling

The VM checks every instruction as it executes: operands must reference valid constants, locals, globals, and jump targets, and operand types must be supported. Failures stop the program with an error code such as `Type error`, `Bounds error`, `Stack error`, or `Invalid opcode`.

{: .warning }
Bytecode is not trusted input. A malformed module is rejected by the loader or stopped by these runtime checks rather than executed.
