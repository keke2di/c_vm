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

Function record, format version 3:

| Field | Size | Description |
|:------|:-----|:------------|
| name index | u32 | Index into the names section |
| local count | u16 | Number of local slots |
| parameter count | u16 | Number of parameters, at most the local count |
| default count | u16 | Number of defaults, at most the parameter count |
| parameter names | u32 × parameter count | Name indexes, in parameter order |
| default values | u32 × default count | Constant indexes for the last parameters |
| code length | u32 | Bytecode length |
| code | code length bytes | Function bytecode, see [Bytecode](BYTECODE.md) |

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

The container format has stayed at version 3 since v0.2.0. Later releases have added instructions and runtime types without changing the container layout or the serialized constant tags, so the loader still accepts format version 3.

{: .note }
The instruction set can change between releases, and when it does, a `.cvm` module runs only on a matching runtime. **v0.3.0 did not change it:** both the container format (3) and the instruction set are unchanged since v0.2.2, so modules compiled with v0.2.2 run on v0.3.0 without recompiling. Packed executables are never affected either way, because each one carries the runtime it was built with.
