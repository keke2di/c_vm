# cVM

## Python-like language runtime

cVM is a small Python-like language runtime built around a custom bytecode compiler, a native C virtual machine, and a standalone Windows executable packer.

**Current release: v0.1.0**

### Start here

- [Getting Started](getting-started.md)
- [Language Reference](LANGUAGE.md)
- [Bytecode](BYTECODE.md)
- [CVM2 Format](CVM2_FORMAT.md)
- [Development](DEVELOPMENT.md)
- [Release Process](RELEASE.md)

### What is cVM?

cVM compiles a deliberately limited subset of Python source into CVM2 bytecode and executes it in a native C virtual machine.

For standalone distribution, compiled programs can be packed together with the native VM into a Windows executable.

### v0.1.0

The first public release focuses on a stable language subset, validated CVM2 loading, hardened native runtime error handling, release build tooling, and regression coverage.

The v0.1.0 test suite currently passes:

```text
55 example tests
4 compiler error tests
9 container validation tests
8 arithmetic error tests

76 passed
0 failed
```

### Repository

See the [cVM GitHub repository](https://github.com/keke2di/c_vm).

### Important

CVM2 does **not** provide cryptographic protection for bytecode. The bytecode payload is stored without encryption.
