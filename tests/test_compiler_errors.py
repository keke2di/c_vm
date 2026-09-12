import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CASES = [
    ("break.py", "break outside loop"),
    ("continue.py", "continue outside loop"),
    ("unsupported.py", "unsupported statement"),
    ("bad_augassign.py", "augmented assignment only supported for simple names"),
]


def run_case(name):
    source = ROOT / "tests" / "compiler_errors" / name
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "compiler.cli",
            str(source),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    return result


def main():
    failed = 0

    for name, expected in CASES:
        result = run_case(name)

        if result.returncode != 1:
            print(f"{name}: FAIL (return code {result.returncode})")
            failed += 1
            continue

        if "compile error:" not in result.stderr:
            print(f"{name}: FAIL (missing compile error)")
            failed += 1
            continue

        if expected not in result.stderr:
            print(f"{name}: FAIL (unexpected error)")
            print(result.stderr, end="")
            failed += 1
            continue

        if result.stdout:
            print(f"{name}: FAIL (unexpected stdout)")
            print(result.stdout, end="")
            failed += 1
            continue

        print(f"{name}: PASS")

    print()
    print(f"{len(CASES) - failed} passed, {failed} failed, {len(CASES)} total")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())