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
    "unknown_builtin_keyword": (
        """
print(1, foo=2)
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
    "missing_dict_key": (
        """
d = {"a": 1}
print(d["a"])
d["b"]
""",
        "Key error",
        "1\n",
    ),
    "delete_missing_dict_key": (
        """
d = {}
del d["x"]
""",
        "Key error",
        "",
    ),
    "unhashable_dict_key": (
        """
d = {}
d[[1]] = 2
""",
        "Type error",
        "",
    ),
    "unhashable_set_item": (
        """
s = {1}
s.add({})
""",
        "Type error",
        "",
    ),
    "unhashable_membership": (
        """
print([1] in [[1]])
[1] in {1: 2}
""",
        "Type error",
        "True\n",
    ),
    "unknown_method": (
        """
x = [1]
x.nope()
""",
        "Attribute error",
        "",
    ),
    "method_keyword_rejected": (
        """
x = []
x.append(item=1)
""",
        "Type error",
        "",
    ),
    "invalid_int_literal": (
        """
print(int("12"))
int("12a")
""",
        "Value error",
        "12\n",
    ),
    "float_to_int_overflow": (
        """
int(float("inf"))
""",
        "Integer overflow",
        "",
    ),
    "dict_changed_during_iteration": (
        """
d = {"a": 1}
for k in d:
    d["b"] = 2
""",
        "Runtime error",
        "",
    ),
    "unknown_encoding": (
        """
"x".encode("bogus")
""",
        "Lookup error",
        "",
    ),
    "unknown_error_handler_used": (
        """
print("x".encode("ascii", "bogus"))
"\\u00e9".encode("ascii", "bogus")
""",
        "Lookup error",
        "b'x'\n",
    ),
    "invalid_utf8": (
        """
b"\\xff".decode()
""",
        "Unicode error",
        "",
    ),
    "decode_with_xmlcharrefreplace": (
        """
b"\\xff".decode("utf-8", "xmlcharrefreplace")
""",
        "Type error",
        "",
    ),
    "bytes_from_str_without_encoding": (
        """
bytes("x")
""",
        "Type error",
        "",
    ),
    "bytes_item_out_of_range": (
        """
bytes([1, 256])
""",
        "Value error",
        "",
    ),
    "decode_str": (
        """
str("x", "utf-8")
""",
        "Type error",
        "",
    ),
    "unhashable_dict_view": (
        """
{{}.keys(): 1}
""",
        "Type error",
        "",
    ),
    "frozenset_has_no_add": (
        """
frozenset().add(1)
""",
        "Attribute error",
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
