import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
COMPILER = "compiler.cli"
PACKER = "packer.pack"
STUB = ROOT / "vm_c" / "stub.exe"
EXAMPLES = ROOT / "examples"
OUTPUT = ROOT / "output"
TIMEOUT = 120
ENV = {**os.environ, "PYTHONUTF8": "1"}


KNOWN_DIFFERENCES = {
    "test_callable_expr.py": (
        "7\n-1\n5\n42\ntruthy\n<function add>\n<built-in function print>\n",
        "function repr has no address",
    ),
    "test_enumerate_simple.py": (
        "<iterator object>\n",
        "iterator repr is generic and has no address",
    ),
}


def run(command, cwd=ROOT):
    return subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=ENV,
        timeout=TIMEOUT,
    )


def compile_example(source):
    output = OUTPUT / f"{source.stem}.cvm"

    result = run([
        sys.executable,
        "-m",
        COMPILER,
        str(source),
        "-o",
        str(output),
    ])

    if result.returncode != 0:
        return False, result.stdout + result.stderr

    return output.exists(), ""


def pack_example(cvm):
    exe = OUTPUT / f"{cvm.stem}.exe"

    result = run([
        sys.executable,
        "-m",
        PACKER,
        str(STUB),
        str(cvm),
        str(exe),
    ])

    if result.returncode != 0:
        return False, result.stdout + result.stderr

    return exe.exists(), ""


def run_example(exe):
    return run([str(exe)])


def run_cpython(source):
    result = run([sys.executable, str(source)])
    return result.stdout, result.returncode != 0


def check_example(source):
    cpython_stdout, cpython_failed = run_cpython(source)

    ok, details = compile_example(source)
    if not ok:
        return False, "compile", details

    cvm = OUTPUT / f"{source.stem}.cvm"

    ok, details = pack_example(cvm)
    if not ok:
        return False, "pack", details

    exe = OUTPUT / f"{source.stem}.exe"
    result = run_example(exe)
    cvm_failed = result.returncode != 0

    expected_stdout = cpython_stdout
    expected_failure = cpython_failed
    note = ""

    known = KNOWN_DIFFERENCES.get(source.name)
    if known is not None:
        if result.stdout == cpython_stdout and cvm_failed == cpython_failed:
            return False, "matches CPython now; remove it from KNOWN_DIFFERENCES", ""
        expected_stdout, reason = known
        expected_failure = False
        note = f"known difference: {reason}"

    if cvm_failed != expected_failure:
        problem = "runtime" if cvm_failed else "expected a runtime failure like CPython"
        return False, problem, result.stdout + result.stderr

    if result.stdout != expected_stdout:
        source_of_truth = "Known cVM output" if known is not None else "CPython"
        return False, "output", (
            f"{source_of_truth}:\n{expected_stdout!r}\ncVM:\n{result.stdout!r}\n"
        )

    return True, note, ""


def main():
    quiet = "--quiet" in sys.argv[1:]

    OUTPUT.mkdir(exist_ok=True)

    if not STUB.exists():
        print(f"stub.exe not found: {STUB}")
        print("Build the VM using the Visual Studio Developer Command Prompt first.")
        return 1

    examples = sorted(EXAMPLES.glob("*.py"))

    if not examples:
        print("No examples found.")
        return 1

    stale = sorted(set(KNOWN_DIFFERENCES) - {source.name for source in examples})

    passed = 0
    failed = 0

    for name in stale:
        failed += 1
        print(f"{name}: FAIL (listed in KNOWN_DIFFERENCES but missing from examples)")

    for source in examples:
        if not quiet:
            print(f"Testing {source.name}...", end=" ", flush=True)

        try:
            ok, note, details = check_example(source)
        except subprocess.TimeoutExpired:
            ok, note, details = False, "timeout", ""

        if ok:
            passed += 1
            if not quiet:
                print(f"PASS ({note})" if note else "PASS")
            continue

        failed += 1
        if quiet:
            print(f"{source.name}: FAIL ({note})")
        else:
            print(f"FAIL ({note})")
        if details:
            print(details.rstrip("\n"))

    total = passed + failed

    print()
    print(f"{passed} passed, {failed} failed, {total} total")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
