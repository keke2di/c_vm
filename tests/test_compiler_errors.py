import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CASES = [
    ("break.py", "break outside loop"),
    ("continue.py", "continue outside loop"),
    ("unsupported.py", "unsupported statement"),
    ("bad_augassign.py", "augmented assignment only supported for simple names"),
    ("star_args_param.py", "*args parameters are not supported yet"),
    ("kwargs_param.py", "**kwargs parameters are not supported yet"),
    ("kwonly_param.py", "keyword-only parameters are not supported yet"),
    ("posonly_param.py", "positional-only parameters are not supported yet"),
    ("non_constant_default.py", "default values must be constant expressions"),
    ("star_call.py", "argument unpacking with * is not supported yet"),
    ("double_star_call.py", "argument unpacking with ** is not supported yet"),
    ("duplicate_keyword.py", "duplicate keyword argument: a"),
    ("duplicate_param.py", "duplicate parameter name"),
    ("decorator.py", "decorators are not supported yet"),
    ("method_keywords.py", "keyword arguments in method calls are not supported yet"),
    ("syntax_error.py", "line 1"),
    ("for_else.py", "for ... else is not supported yet"),
    ("while_else.py", "while ... else is not supported yet"),
    ("dict_unpack.py", "dict unpacking with ** is not supported yet"),
    ("type_params.py", "type parameters are not supported"),
    ("module_return.py", "'return' outside function"),
    ("python_version", "cVM requires Python 3.14, running 3.13"),
]


VERSION_PROBE = (
    "import sys\n"
    "sys.version_info = (3, 13, 0, 'final', 0)\n"
    "from compiler.cli import main\n"
    "raise SystemExit(main(['tests/compiler_errors/unsupported.py']))\n"
)


def run_case(name):
    if name == "python_version":
        command = [sys.executable, "-c", VERSION_PROBE]
    else:
        source = ROOT / "tests" / "compiler_errors" / name
        command = [sys.executable, "-m", "compiler.cli", str(source)]
    return subprocess.run(command, cwd=ROOT, capture_output=True, text=True)


def main():
    quiet = "--quiet" in sys.argv[1:]
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

        if not quiet:
            print(f"{name}: PASS")

    print()
    print(f"{len(CASES) - failed} passed, {failed} failed, {len(CASES)} total")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())