import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
COMPILER = "compiler.cli"
PACKER = "packer.pack"
STUB = ROOT / "vm_c" / "stub.exe"


CASES = {
    "call_non_callable": (
        """
print(1)
x = 0
x(5)
""",
        "Type error",
        "1\n",
    ),
    "call_string": (
        """
x = "abc"
x(1)
""",
        "Type error",
        "",
    ),
    "too_many_args": (
        """
def f(a):
    return a

f(1, 2)
""",
        "Type error",
        "",
    ),
    "too_few_args": (
        """
def f(a, b):
    return a

f(1)
""",
        "Type error",
        "",
    ),
    "builtin_bad_arity": (
        """
str(1, 2)
""",
        "Type error",
        "",
    ),
    "undefined_function": (
        """
missing(1)
""",
        "Function not found",
        "",
    ),
    "unknown_keyword": (
        """
def f(a):
    return a

f(b=1)
""",
        "Type error",
        "",
    ),
    "duplicate_argument": (
        """
def f(a, b):
    return a

f(1, a=2)
""",
        "Type error",
        "",
    ),
    "missing_required_argument": (
        """
def f(a, b=2):
    return a

f(b=3)
""",
        "Type error",
        "",
    ),
    "too_many_positional_with_defaults": (
        """
def f(a, b=2):
    return a

f(1, 2, 3)
""",
        "Type error",
        "",
    ),
    "keyword_to_builtin": (
        """
print(1, end="")
""",
        "Type error",
        "",
    ),
    "keyword_call_non_callable": (
        """
x = 5
x(a=1)
""",
        "Type error",
        "",
    ),
}


def run(command):
    return subprocess.run(
        command,
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


def check_case(temp, name, source_text, expected_error, expected_stdout):
    source = temp / f"{name}.py"
    cvm = temp / f"{name}.cvm"
    exe = temp / f"{name}.exe"

    source.write_text(source_text.strip() + "\n", encoding="utf-8")

    result = run([sys.executable, "-m", COMPILER, str(source), "-o", str(cvm)])
    if result.returncode != 0:
        return "compile", result.stderr

    result = run([sys.executable, "-m", PACKER, str(STUB), str(cvm), str(exe)])
    if result.returncode != 0:
        return "pack", result.stderr

    result = run([str(exe)])

    if result.returncode == 0:
        return "expected runtime error", result.stdout
    if expected_error not in result.stderr:
        return "wrong runtime error", result.stderr
    if result.stdout != expected_stdout:
        return "unexpected stdout", f"expected {expected_stdout!r}, got {result.stdout!r}"

    return None, ""


def main():
    quiet = "--quiet" in sys.argv[1:]

    if not STUB.exists():
        print(f"stub.exe not found: {STUB}")
        print("Build the VM first.")
        return 1

    passed = 0
    failed = 0

    with tempfile.TemporaryDirectory(prefix="cvm_runtime_") as temp_dir:
        temp = Path(temp_dir)

        for name, (source_text, expected_error, expected_stdout) in CASES.items():
            problem, details = check_case(
                temp, name, source_text, expected_error, expected_stdout
            )

            if problem is None:
                passed += 1
                if not quiet:
                    print(f"{name}: PASS")
                continue

            failed += 1
            print(f"{name}: FAIL ({problem})")
            if details:
                print(details.rstrip("\n"))

    total = passed + failed

    print()
    print(f"{passed} passed, {failed} failed, {total} total")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
