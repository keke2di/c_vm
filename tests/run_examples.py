import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
COMPILER = "compiler.cli"
PACKER = "packer.pack"
STUB = ROOT / "vm_c" / "stub.exe"
EXAMPLES = ROOT / "examples"
OUTPUT = ROOT / "output"


EXPECTED_OUTPUT = {
    "test_app.py": "30\n",
    "test_append.py": "4\n",
    "test_augassign.py": "15\n",
    "test_boolop.py": "1\n",
    "test_builtins.py": "52\n",
    "test_const.py": "579\n",
    "test_contains.py": "2\n",
    "test_continue.py": "18\nNone\n",
    "test_continue_for.py": "12\nNone\n",
    "test_continue_for_list.py": "12\nNone\n",
    "test_dict.py": "1\n",
    "test_dict_iteration.py": "a\nb\nc\nNone\n",
    "test_dict_comp.py": "4\n",
    "test_dict_literal.py": "6\n",
    "test_enumerate.py": "[list len=2]\n[list len=2]\n[list len=2]\n0\n",
    "test_enumerate_simple.py": "[list len=3]\n",
    "test_expression_integration.py": "42\n-2\n42\nNone\n",
    "test_float.py": "5.14\n",
    "test_float_builtin.py": "3.14\n",
    "test_for.py": "10\n",
    "test_for_break.py": "10\n",
    "test_while.py": "3\n10\nNone\n",
    "test_func.py": "30\n",
    "test_recursion.py": "120\nNone\n",
    "test_recursion_deep.py": "50\nNone\n",
    "test_int_str.py": "124\n",
    "test_integration_basic.py": "60\ni\negrat\nNone\n",
    "test_integration_collections.py": "3\n2\n1\n0\nNone\n",
    "test_list.py": "4\n",
    "test_list_comp.py": "7\n",
    "test_list_index.py": "20\n",
    "test_nested_continue_outer.py": "42\nNone\n",
    "test_nested_loops.py": "9\nNone\n",
    "test_nested_while.py": "8\nNone\n",
    "test_return_in_loop.py": "4\n0\nNone\n",
    "test_integration_patterns.py": "9\nhw\n60\nNone\n",
    "test_mod.py": "1\nNone\n",
    "test_negative_index.py": "10\n40\n40\n30\n10\nNone\n",
    "test_list_literal.py": "3\n",
    "test_plain_list.py": "7\n",
    "test_print.py": "42\n0\n",
    "test_set_and_for.py": "9\n",
    "test_set_comp.py": "3\n",
    "test_str_return.py": "Hello World\n",
    "test_strings.py": "Hello World\n0\n",
    "test_string_index.py": "H\ne\no\no\nl\nNone\n",
    "test_string_slice.py": "ello\nHello\nWorld\nHello World\nHloWrd\ndlroW olleH\nNone\n",
    "test_tuple.py": "3\n",
    "test_vars.py": "30\n",
    "tet_return_int.py": "42\n",
    "studio_showcase.py": '28\n17\nhell\n56\n56\n',
    "test_floordiv.py": '3\n-4\nNone\n',
    "test_pow.py": '256\n27\nNone\n',
    "test_unary.py": '42\n3.14\n-6\n5\nNone\n',
    "test_slice.py": "[list len=3]\n"
                     "[list len=3]\n"
                     "[list len=4]\n"
                     "[list len=6]\n"
                     "[list len=2]\n"
                     "[list len=6]\n"
                     "None\n",
}


def run(command, cwd=ROOT):
    return subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
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
        print(result.stdout, end="")
        print(result.stderr, end="")
        return False

    return output.exists()


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
        print(result.stdout, end="")
        print(result.stderr, end="")
        return False

    return exe.exists()


def run_example(exe):
    return run([str(exe)])


def main():
    OUTPUT.mkdir(exist_ok=True)

    if not STUB.exists():
        print(f"stub.exe not found: {STUB}")
        print("Build the VM using the Visual Studio Developer Command Prompt first.")
        return 1

    examples = sorted(EXAMPLES.glob("*.py"))

    if not examples:
        print("No examples found.")
        return 1

    passed = 0
    failed = 0

    for source in examples:
        print(f"Testing {source.name}...", end=" ")

        expected = EXPECTED_OUTPUT.get(source.name)
        expected_runtime_failure = source.name == "test_recursion_limit.py"

        if expected is None and not expected_runtime_failure:
            print("FAIL (missing expected output)")
            failed += 1
            continue

        if not compile_example(source):
            print("FAIL (compile)")
            failed += 1
            continue

        cvm = OUTPUT / f"{source.stem}.cvm"

        if not pack_example(cvm):
            print("FAIL (pack)")
            failed += 1
            continue

        exe = OUTPUT / f"{source.stem}.exe"
        result = run_example(exe)

        if result.returncode != 0:
            if source.name == "test_recursion_limit.py":
                print("PASS (expected runtime limit)")
                passed += 1
                continue

            print("FAIL (runtime)")
            if result.stdout:
                print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, end="")
            failed += 1
            continue

        if result.stdout != expected:
            print("FAIL (output)")
            print("Expected:")
            print(repr(expected))
            print("Actual:")
            print(repr(result.stdout))
            failed += 1
            continue

        print("PASS")
        passed += 1

    total = passed + failed

    print()
    print(f"{passed} passed, {failed} failed, {total} total")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())