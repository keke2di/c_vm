---
title: CVM2 Format
nav_order: 6
description: "Layout of CVM2 containers, compiled modules, and packed executables."
---

# CVM2 Format
{: .no_toc }

The layout of `.cvm` containers, the module inside them, and packed executables.
{: .fs-6 .fw-300 }

<details open markdown="block">
  <summary>
    On this page
  </summary>
  {: .text-delta }
1. TOC
{:toc}
</details>

{: .warning }
CVM2 is **not** an encryption format. The bytecode payload is stored in plain form, and anyone with a `.cvm` file or packed executable can inspect or extract it.

## Overview

A `.cvm` file has three parts:

```text
┌──────────────────────────────┐
│ header             40 bytes  │
│ nonce              12 bytes  │
│ module payload               │
└──────────────────────────────┘
```

All integers are little-endian.

## Container header

The header matches the Python `struct` format `<4sBBHIIII16s`, followed by the nonce.

| Offset | Size | Field | Value |
|-------:|-----:|:------|:------|
| 0 | 4 | magic | `CVM2` |
| 4 | 1 | format version | `3` |
| 5 | 1 | pepper ID | `1` |
| 6 | 2 | flags | `0` |
| 8 | 4 | plaintext length | Payload length |
| 12 | 4 | payload length | Must equal the plaintext length |
| 16 | 4 | salt length | `16` |
| 20 | 4 | nonce length | `12` |
| 24 | 16 | salt | Reserved, written as zeros |
| 40 | 12 | nonce | Reserved, written as zeros |
| 52 | variable | payload | Serialized module; must end exactly at the end of the file |

The pepper, salt, and nonce fields are kept for layout compatibility and are not used to encrypt anything.

## Module payload

| Field | Size |
|:------|:-----|
| Entry function index | u32 |
| Constants section | tag `0x01` |
| Names section | tag `0x02` |
| Functions section | tag `0x03` |

Each section is a 1-byte tag, a u32 length, and that many bytes of data. The sections appear in this order and must fill the payload exactly.

### Constants section

A u32 count, followed by that many tagged constants:

| Tag | Type | Data |
|:----|:-----|:-----|
| `0x00` | `None` | none |
| `0x01` | `False` | none; loads as the integer `0` |
| `0x02` | `True` | none; loads as the integer `1` |
| `0x03` | `int` | i64 |
| `0x04` | `float` | f64 |
| `0x05` | `str` | u32 length, then UTF-8 bytes |
| `0x06` | `bytes` | u32 length, then the bytes |
| `0x07` | `tuple` | u32 item count, then that many tagged constants |

A module holds at most 65536 constants. The compiler limits strings and bytes to 1 MiB and tuples to 65536 items.

### Names section

A u32 count (at most 65535), followed by that many names. Each name is a u8 length and up to 255 UTF-8 bytes.

The names section holds globals, function names, and parameter names.

### Functions section

A u32 function count (1 to 4096), followed by one record per function.

Function record, format version 4:

| Field | Size | Description |
|:------|:-----|:------------|
| name index | u32 | Index into the names section |
| local count | u16 | Number of local slots |
| parameter count | u16 | Number of positional parameters (positional-only plus positional-or-keyword) |
| positional-only count | u16 | How many of the positional parameters are positional-only |
| default count | u16 | Number of defaults, at most the parameter count |
| keyword-only count | u16 | Number of keyword-only parameters |
| `*args` slot | u16 | Local slot of `*args`, or `0xFFFF` |
| `**kwargs` slot | u16 | Local slot of `**kwargs`, or `0xFFFF` |
| cell count | u16 | Number of locals this function owns a cell for |
| free count | u16 | Number of captured free variables |
| parameter names | u32 × parameter count | Name indexes, in parameter order |
| keyword-only names | u32 × keyword-only count | Name indexes |
| keyword-only slots | u16 × keyword-only count | Local slot of each keyword-only parameter |
| keyword-only flags | u8 × keyword-only count | 1 when the parameter has a default |
| cell slots | u16 × cell count | Local slot each cell replaces |
| free names | u32 × free count | Names captured from enclosing functions, in closure order |
| line count | u32 | Number of entries in the line table |
| line table | (u32 offset, u32 line) × line count | Byte offset of the first instruction on a new source line, paired with that line |
| code length | u32 | Bytecode length |
| code | code length bytes | Function bytecode, see [Bytecode](BYTECODE.md) |

Default *values* are no longer stored in the record: the `MAKE_FUNCTION` instruction builds them at definition time, so a default may be any expression.

### Source section

Section `0x04` holds the source file name as UTF-8 bytes. The runtime uses it, with the line tables, to print tracebacks.

## Packed executables

The packer appends a container and a 16-byte trailer to the VM stub:

| Part | Size |
|:-----|:-----|
| VM stub (`stub.exe`) | stub size |
| `.cvm` container | container size |
| Container offset | u64 |
| Container length | u32 |
| Trailer magic `CVMT` | 4 bytes |

At startup, the stub reads the trailer from the end of its own file, checks the magic, and loads the container from the recorded offset.

## Validation

The loader checks, before anything runs:

- Magic, format version, pepper ID, and flags
- Header field lengths and payload bounds
- Declared lengths against physical lengths, including trailing data
- Section tags and section lengths
- Constant encodings
- Name indexes for functions and parameters
- Constant indexes for default values
- Parameter and default counts
- Entry function index

The VM checks, while it runs:

- Opcodes
- Constant, local, and global references
- Jump targets
- Stack bounds
- Call targets and argument binding

Malformed containers are rejected rather than executed.

## Compatibility

The runtime accepts only the format version it was built for.

| Format version | cVM | Change |
|:---------------|:----|:-------|
| 2 | v0.1.0 | First public format |
| 3 | v0.2.0 | Function records carry a default count, parameter names, and default values |
| 4 | v0.4.0 (unreleased) | Function records carry every parameter kind, cell slots, and free-variable names; default values moved to `MAKE_FUNCTION` |
| 5 | v0.4.0 (unreleased) | Function records carry a line table, and a new source section records the file name |

The container format stayed at version 3 from v0.2.0 through v0.3.0. Later releases added instructions and runtime types without changing the container layout or the serialized constant tags, so the loader kept accepting format version 3.

{: .note }
The instruction set can change between releases, and when it does, a `.cvm` module runs only on a matching runtime. **v0.3.0 did not change it:** both the container format (3) and the instruction set are unchanged since v0.2.2, so modules compiled with v0.2.2 run on v0.3.0 without recompiling. Packed executables are never affected either way, because each one carries the runtime it was built with.

The unreleased work after v0.3.0 first added two instructions, `STORE_SLICE` (`0x66`) and `DELETE_SLICE` (`0x67`), and moved the opcode table to 9 without touching the container. Functions, scopes and closures then changed the function record itself, so **the container format moved to version 4** (opcode table 10 for `LOAD_DEREF`/`STORE_DEREF`/`DELETE_DEREF`, `LOAD_CLOSURE`, `MAKE_FUNCTION`, `CALL_EX`, `LIST_EXTEND`, `DICT_MERGE`), and exceptions added a per-function line table plus a source section, making it **version 5** (opcode table 11 for `SETUP_HANDLER`, `POP_HANDLER`, `EXCEPT_MATCH`, `EXCEPT_CLEAR`, `RERAISE`, `RAISE_VARARGS`). Modules compiled by v0.3.0 or earlier no longer load.
