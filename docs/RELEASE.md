---
title: Release Process
nav_order: 8
description: "How a cVM release is prepared and published."
---

# Release Process
{: .no_toc }

How a cVM release is prepared and published.
{: .fs-6 .fw-300 }

<details open markdown="block">
  <summary>
    On this page
  </summary>
  {: .text-delta }
1. TOC
{:toc}
</details>

## Versioning

| Change | Version bump | Example |
|:-------|:-------------|:--------|
| Compatible fixes and features | Patch | `0.2.0` to `0.2.1` |
| Bytecode, container, or runtime semantics change | Minor | `0.2.0` to `0.3.0` |

{: .note }
**One recorded exception.** `v0.2.2` shipped bytecode and runtime changes under a patch label, which this table would call a minor bump. That was a deliberate choice to publish a mid-development checkpoint, not a change to the rule above. The rule stands as written for every release after it.

## Checklist

### Source tree

- Remove generated executables and build intermediates.
- Remove Python cache directories.
- Confirm only intended source, tests, examples, documentation, and project metadata remain.

### Build

From an MSVC Developer Command Prompt or Developer PowerShell:

```powershell
python vm_c/build.py --release
```

The release build should complete without compiler warnings or errors.

### Tests

```powershell
python tests/run_all.py
```

Every suite must pass.

### Documentation

- Update the version in `README.md` and on the documentation home page.
- Update [Compatibility](COMPATIBILITY.md) and the [Language Reference](LANGUAGE.md) for any behavior change.
- Add a [Changelog](CHANGELOG.md) entry.

### Review

```powershell
git diff --check
git status --short
```

Review the complete working tree before committing.

## Publishing

```powershell
git add .
git status --short
git diff --cached --check
git commit -m "Release cVM v0.3.0"
git tag -a v0.3.0 -m "cVM v0.3.0"
git push origin main
git push origin v0.3.0
```

Create the GitHub release from the tag, using the changelog entry as release notes.

{: .note }
This site is built by GitHub Pages from the `docs/` folder on `main`, so documentation changes go live with the push.

## Release artifacts

A standalone program is built with:

```powershell
python -m compiler.cli examples/test_app.py -o output/test_app.cvm
python -m packer.pack vm_c/stub.exe output/test_app.cvm output/test_app.exe
```

Generated artifacts stay out of the source tree unless they are intentionally attached to a GitHub release.

{: .warning }
CVM2 does not provide cryptographic bytecode protection. Release notes and documentation must keep saying so.
