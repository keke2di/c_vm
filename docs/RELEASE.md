# Release Process

## cVM v0.1.0

The first public release is intended to be a stable baseline for the compiler, CVM2 format, native VM, and standalone packer.

## Pre-release checklist

### Source tree

- Remove generated executables and build intermediates.
- Remove Python cache directories.
- Confirm only intended source, tests, examples, documentation, and project metadata remain.
- Confirm the version is `0.1.0`.

### Build

From an MSVC Developer Command Prompt or Developer PowerShell:

```powershell
python vm_c/build.py --release
```

The release build should complete without compiler warnings or errors.

### Tests

Run:

```powershell
python tests/run_examples.py
python tests/test_compiler_errors.py
python tests/test_container_errors.py
python tests/test_arithmetic_errors.py
```

All release tests should pass.

### Validation

Run:

```powershell
git diff --check
git status --short
```

Review the complete working tree before committing.

## Release artifact

A standalone program is created by:

```powershell
python -m compiler.cli examples/test_app.py -o output/test_app.cvm
python -m packer.pack vm_c/stub.exe output/test_app.cvm output/test_app.exe
```

Generated artifacts should remain outside the committed source tree unless they are intentionally attached to a GitHub release.

## Versioning

The public release tag is:

```text
v0.1.0
```

The release commit should represent the complete source tree that was tested.

## Git release sequence

After the final source and documentation review:

```powershell
git add .
git status --short
git diff --cached --check
git commit -m "Release cVM v0.1.0"
git tag -a v0.1.0 -m "cVM v0.1.0"
git push -u origin main
git push origin v0.1.0
```

The GitHub release should be created from the `v0.1.0` tag.

## Release notes

Release notes should describe:

- The first public cVM release
- The supported language subset
- CVM2 container behavior
- Native VM/runtime hardening
- Test coverage
- Any intentionally unsupported functionality

CVM2 does not provide cryptographic bytecode protection. This should remain explicit in public release documentation.
