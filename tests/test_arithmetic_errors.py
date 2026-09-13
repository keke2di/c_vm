import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
COMPILER = "compiler.cli"
PACKER = "packer.pack"
STUB = ROOT / "vm_c" / "stub.exe"


CASES = {
    "add_overflow": """
x = 9223372036854775807
x = x + 1
""",
    "sub_overflow": """
x = -9223372036854775807
x = x - 2
""",
    "mul_overflow": """
x = 3037000500
x = x * x
""",
    "div_overflow": """
x = -9223372036854775807
x = x - 1
x = x / -1
""",
    "mod_overflow": """
x = -9223372036854775807
x = x - 1
x = x % -1
""",
    "floordiv_overflow": """
x = -9223372036854775807
x = x - 1
x = x // -1
""",
    "pow_overflow": """
x = 2
x = x ** 63
""",
    "neg_overflow": """
x = -9223372036854775807
x = x - 1
x = -x
""",
}


def run(command):
    return subprocess.run(
        command,
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


def main():
    quiet = "--quiet" in sys.argv[1:]

    if not STUB.exists():
        print(f"stub.exe not found: {STUB}")
        print("Build the VM first.")
        return 1

    passed = 0
    failed = 0

    with tempfile.TemporaryDirectory(prefix="cvm_arithmetic_") as temp_dir:
        temp = Path(temp_dir)

        for name, source_text in CASES.items():
            source = temp / f"{name}.py"
            cvm = temp / f"{name}.cvm"
            exe = temp / f"{name}.exe"

            source.write_text(source_text.strip() + "\n", encoding="utf-8")

            compile_result = run([
                sys.executable,
                "-m",
                COMPILER,
                str(source),
                "-o",
                str(cvm),
            ])

            if compile_result.returncode != 0:
                print(f"{name}: FAIL (compile)")
                print(compile_result.stderr, end="")
                failed += 1
                continue

            pack_result = run([
                sys.executable,
                "-m",
                PACKER,
                str(STUB),
                str(cvm),
                str(exe),
            ])

            if pack_result.returncode != 0:
                print(f"{name}: FAIL (pack)")
                print(pack_result.stderr, end="")
                failed += 1
                continue

            result = run([str(exe)])

            if result.returncode == 0:
                print(f"{name}: FAIL (expected runtime overflow)")
                if result.stdout:
                    print(result.stdout, end="")
                failed += 1
                continue

            if "Integer overflow" not in result.stderr:
                print(f"{name}: FAIL (wrong runtime error)")
                if result.stderr:
                    print(result.stderr, end="")
                failed += 1
                continue

            if not quiet:
                print(f"{name}: PASS")
            passed += 1

    total = passed + failed

    print()
    print(f"{passed} passed, {failed} failed, {total} total")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())