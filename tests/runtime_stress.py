import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
COMPILER = "compiler.cli"
PACKER = "packer.pack"
ASAN_STUB = ROOT / "vm_c" / "asan_stub.exe"
STUB = ASAN_STUB if ASAN_STUB.exists() else ROOT / "vm_c" / "stub.exe"

CASES = {
    "deep_recursion": (
        """
def f(n):
    if n == 0:
        return 0
    return f(n - 1)

print(f(1000))
""",
        "0\n",
        True,
    ),
    "large_list": (
        """
x = []
i = 0
while i < 10000:
    x.append(i)
    i += 1
print(len(x))
""",
        "10000\n",
        False,
    ),
    "large_string": (
        """
x = ""
i = 0
while i < 100000:
    x = x + "a"
    i += 1
print(len(x))
""",
        "100000\n",
        False,
    ),
    "nested_collections": (
        """
x = []
i = 0
while i < 1000:
    x.append([i, i + 1, i + 2])
    i += 1
print(len(x))
""",
        "1000\n",
        False,
    ),
    "repeated_calls": (
        """
def f(x):
    return x + 1

x = 0
i = 0
while i < 100000:
    x = f(x)
    i += 1
print(x)
""",
        "100000\n",
        False,
    ),
}


def run(command, cwd=ROOT):
    return subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
    )


def run_case(name, source, expected, expect_failure):
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        source_path = tmp_path / f"{name}.py"
        cvm_path = tmp_path / f"{name}.cvm"
        exe_path = tmp_path / f"{name}.exe"

        source_path.write_text(source.strip() + "\n", encoding="utf-8")

        result = run([
            sys.executable,
            "-m",
            COMPILER,
            str(source_path),
            "-o",
            str(cvm_path),
        ])

        if result.returncode != 0:
            return False, f"compile failed:\n{result.stderr}"

        result = run([
            sys.executable,
            "-m",
            PACKER,
            str(STUB),
            str(cvm_path),
            str(exe_path),
        ])

        if result.returncode != 0:
            return False, f"pack failed:\n{result.stderr}"

        try:
            result = run([str(exe_path)])
        except subprocess.TimeoutExpired:
            return False, "timeout"

        if expect_failure:
            if result.returncode == 0:
                return False, "expected runtime failure"
            return True, ""

        if result.returncode != 0:
            return False, (
                f"runtime failed:\n"
                f"return code: {result.returncode}\n"
                f"stdout: {result.stdout!r}\n"
                f"stderr: {result.stderr!r}"
            )

        if result.stdout != expected:
            return False, f"expected {expected!r}, got {result.stdout!r}"

        return True, ""


def main():
    quiet = "--quiet" in sys.argv[1:]
    passed = 0

    for name, (source, expected, expect_failure) in CASES.items():
        try:
            ok, error = run_case(name, source, expected, expect_failure)
        except subprocess.TimeoutExpired:
            ok = False
            error = "timeout"

        if ok:
            if not quiet:
                print(f"{name}: PASS")
            passed += 1
        else:
            print(f"{name}: FAIL")
            print(error)

    print()
    print(f"{passed} passed, {len(CASES) - passed} failed, {len(CASES)} total")
    return 0 if passed == len(CASES) else 1


if __name__ == "__main__":
    sys.exit(main())