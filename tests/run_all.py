import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "tests"

SUITES = [
    "run_examples.py",
    "test_compiler_errors.py",
    "test_container_errors.py",
    "test_arithmetic_errors.py",
    "test_runtime_errors.py",
    "test_stub.py",
    "runtime_stress.py",
]

SUMMARY = re.compile(r"^(\d+) passed, (\d+) failed, (\d+) total$")


def run_suite(script):
    result = subprocess.run(
        [sys.executable, str(TESTS / script), "--quiet"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )

    summary = None
    details = []

    for line in result.stdout.splitlines():
        match = SUMMARY.match(line.strip())
        if match:
            summary = match
        elif line.strip():
            details.append(line)

    details.extend(line for line in result.stderr.splitlines() if line.strip())

    if summary is None:
        details.append(f"no summary (exit code {result.returncode})")
        return 0, 1, details

    passed = int(summary.group(1))
    failed = int(summary.group(2))

    if result.returncode != 0 and failed == 0:
        details.append(f"exit code {result.returncode}")
        failed = 1

    return passed, failed, details


def main():
    total_passed = 0
    total_failed = 0

    for script in SUITES:
        passed, failed, details = run_suite(script)
        total_passed += passed
        total_failed += failed

        if failed:
            print(f"[{Path(script).stem}] {failed} failed")
            for line in details:
                print(f"  {line}")

    print(f"{total_passed} passed, {total_failed} failed")
    return 0 if total_failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
