---
title: Getting Started
nav_order: 2
description: "Build the native VM, compile a program, and pack it into a standalone executable."
---

# Getting Started
{: .no_toc }

Build the VM, compile a program, and turn it into a standalone Windows executable.
{: .fs-6 .fw-300 }

<details open markdown="block">
  <summary>
    On this page
  </summary>
  {: .text-delta }
1. TOC
{:toc}
</details>

## Requirements

- Windows
- Python 3
- Microsoft C/C++ build tools (Visual Studio or Visual Studio Build Tools)

{: .note }
Run the build from an **MSVC Developer Command Prompt** or **Developer PowerShell**. In a regular shell, load the environment first by calling `vcvars64.bat` from your Visual Studio installation.

## 1. Build the native VM

From the repository root:

```powershell
python vm_c/build.py --release
```

This produces `vm_c/stub.exe`, the native VM that runs compiled programs.

## 2. Write a program

Save this as `hello.py`:

```python
def greet(name, punctuation="!"):
    return "Hello " + name + punctuation

print(greet("cVM"))
```

## 3. Compile it

```powershell
python -m compiler.cli hello.py -o output/hello.cvm
```

The `.cvm` file is a compiled CVM2 module. Without `-o`, the compiler writes `hello.cvm` next to the source file.

## 4. Pack a standalone executable

```powershell
python -m packer.pack vm_c/stub.exe output/hello.cvm output/hello.exe
```

The executable contains the native VM and your compiled program.

## 5. Run it

```powershell
.\output\hello.exe
```

```text
Hello cVM!
```

The program produces output through `print`. A `return` statement outside a function is a compile error.

## Run the tests

```powershell
python tests/run_all.py
```

Only failing tests and the total count are printed. See [Development](DEVELOPMENT.md#tests) for the individual suites.

## Troubleshooting

| Message | What to do |
|:--------|:-----------|
| `VCToolsInstallDir not set. Run from Developer Command Prompt.` | The MSVC environment is not loaded. See the note under [Requirements](#requirements). |
| `stub.exe not found` | Build the VM first, as in step 1 above. |
| `compile error: ...` | The source uses something outside the supported subset. Check [Compatibility](COMPATIBILITY.md). |
| `VM run error: Type error` | A runtime type error, such as calling a value that is not a function or comparing unsupported types. |
| `VM load error: Unsupported version` | The module was compiled by a different cVM version. Recompile it. |

## Next steps

- Read the [Language Reference](LANGUAGE.md) to see what you can write.
- Check [Compatibility](COMPATIBILITY.md) for differences from Python.
