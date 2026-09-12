# CVM2 Container Format

## Overview

CVM2 is the container format used by cVM compiled modules.

A `.cvm` file contains the data required to reconstruct a compiled module for the native VM.

The format is designed to provide deterministic structure and strict validation before execution.

**CVM2 is not an encryption format.** The bytecode payload is stored without cryptographic protection.

## Header

The current header is represented by:

```text
<4sBBHIIII16s
```

in little-endian form.

The fields are:

| Field | Size | Description |
|---|---:|---|
| magic | 4 bytes | `CVM2` |
| format version | 1 byte | Current format version |
| pepper ID | 1 byte | Format compatibility field |
| flags | 2 bytes | Container flags |
| plaintext length | 4 bytes | Bytecode/module payload length |
| payload length | 4 bytes | Stored payload length |
| salt length | 4 bytes | Salt field length |
| nonce length | 4 bytes | Nonce field length |
| salt | 16 bytes | Reserved container field |

A nonce field follows the fixed header.

The current implementation retains the pepper, salt, nonce, and length fields as part of the CVM2 layout, but does not use them to encrypt the payload.

## Payload

The payload contains the serialized cVM module.

It includes the information required by the loader for:

- Constants
- Global/name data
- Function metadata
- Function bytecode
- Entry-function information

The compiler creates this module before wrapping it in the CVM2 container.

## Validation

The native loader performs structural validation before execution.

Validation includes checks for:

- Correct magic
- Supported format version
- Supported pepper ID
- Supported flags
- Valid field lengths
- Payload bounds
- Matching declared and physical lengths
- Valid constant indexes
- Valid function indexes
- Valid local references
- Valid jump targets
- Valid opcodes
- Trailing physical data

Malformed containers are rejected rather than executed.

## Compatibility

CVM2 is versioned. A runtime should reject container versions it does not understand.

Changes to the serialized layout, bytecode representation, or metadata semantics may require a new format version.

## Security note

CVM2 provides structural validation, not confidentiality.

Anyone with access to a `.cvm` file or packed executable should assume that its bytecode can be inspected or extracted.
