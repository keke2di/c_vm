# Getting Started

## 1. Build the native VM

Open an MSVC Developer Command Prompt or Developer PowerShell.

From the repository root:

```powershell
python vm_c/build.py --release
```

This produces:

```text
vm_c/stub.exe
```

## 2. Compile a program

Compile one of the included examples:

```powershell
python -m compiler.cli examples/test_app.py -o output/test_app.cvm
```

The resulting `.cvm` file is a CVM2 compiled module.

## 3. Pack a standalone executable

Use the native VM stub and compiled module:

```powershell
python -m packer.pack vm_c/stub.exe output/test_app.cvm output/test_app.exe
```

The result is a standalone Windows executable.

## 4. Run it

From PowerShell:

```powershell
.\output\test_app.exe
```

## 5. Run the tests

The main regression suite:

```powershell
python tests/run_examples.py
```

Compiler errors:

```powershell
python tests/test_compiler_errors.py
```

Container validation:

```powershell
python tests/test_container_errors.py
```

Arithmetic errors:

```powershell
python tests/test_arithmetic_errors.py
```

## What cVM supports

cVM implements a deliberately limited Python-like subset.

Supported functionality includes variables, functions, conditionals, loops, collections, indexing, slicing, comprehensions, arithmetic, comparisons, boolean operations, and selected built-ins/method calls.

Unsupported Python syntax is rejected by the compiler.

## First program

A minimal program can look like:

```python
x = 10
y = 20
print(x + y)
```

Compile and pack it using the same commands above.

The purpose of v0.1.0 is to provide a stable foundation rather than full Python compatibility.
