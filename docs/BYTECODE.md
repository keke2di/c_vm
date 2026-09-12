# cVM Bytecode

## Overview

cVM compiles the supported Python-like source subset into a compact custom bytecode instruction stream.

The native runtime executes this bytecode directly.

The bytecode is stored inside a CVM2 module together with constants, names, and function metadata.

## Instructions

The compiler emits instructions by mnemonic. The native VM uses the corresponding opcode definitions in `vm_c/opcodes.h`.

The current instruction set covers:

- Constants and stack operations
- Local and global variable access
- Arithmetic
- Comparisons
- Boolean operations
- Unary operations
- Control flow
- Function calls and returns
- Method calls
- Collection construction
- Indexing and slicing
- Collection mutation
- Length and iteration operations

The exact opcode values are an implementation detail of the CVM2 bytecode format and are defined by the source in `compiler/opcodes.py` and `vm_c/opcodes.h`.

## Stack model

Most instructions operate on values on the VM operand stack.

For example, a binary arithmetic instruction consumes two operands and leaves the resulting value on the stack.

Conceptually:

```text
before:

    ...  left  right

BINARY_ADD

after:

    ...  result
```

Values are reference-counted by the native runtime. Instructions that consume values transfer or release ownership according to the runtime operation.

## Functions

Compiled functions have metadata describing:

- Function name
- Bytecode offset
- Local-variable count
- Parameter count

A function call creates a native VM frame. The frame tracks the return instruction pointer, locals, associated function metadata, and stack base.

## Control flow

Conditional branches and loops are compiled into absolute bytecode jumps.

The compiler uses labels during emission and resolves them into bytecode offsets during finalization.

## Error handling

The native VM validates bytecode references while loading and executing.

Invalid instructions, invalid indexes, stack failures, type errors, and other runtime failures are reported through VM error codes.

Malformed bytecode is not expected to be trusted input.

## Versioning

The CVM2 container format identifies its format version separately from the implementation.

Changes to opcode encoding or bytecode semantics should therefore be treated as compatibility-sensitive changes.
