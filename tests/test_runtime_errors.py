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
    "list_pop_empty": (
        """
[].pop()
""",
        "Bounds error",
        "",
    ),
    "list_remove_missing": (
        """
print([1, 2].remove(2))
[1, 2].remove(9)
""",
        "Value error",
        "None\n",
    ),
    "list_index_missing": (
        """
[1, 2, 3].index(9)
""",
        "Value error",
        "",
    ),
    "set_remove_missing": (
        """
{1, 2}.remove(9)
""",
        "Key error",
        "",
    ),
    "set_pop_empty": (
        """
set().pop()
""",
        "Key error",
        "",
    ),
    "dict_popitem_empty": (
        """
{}.popitem()
""",
        "Key error",
        "",
    ),
    "set_op_non_set": (
        """
print({1, 2} | {3})
{1} & [2]
""",
        "Type error",
        "{1, 2, 3}\n",
    ),
    "dict_or_non_dict": (
        """
{"a": 1} | [1]
""",
        "Type error",
        "",
    ),
    "split_empty_sep": (
        """
print("a,b".split(","))
"abc".split("")
""",
        "Value error",
        "['a', 'b']\n",
    ),
    "rsplit_empty_sep": (
        """
"abc".rsplit("")
""",
        "Value error",
        "",
    ),
    "partition_empty_sep": (
        """
"abc".partition("")
""",
        "Value error",
        "",
    ),
    "join_non_str": (
        """
print(",".join(["a", "b"]))
",".join(["a", 1])
""",
        "Type error",
        "a,b\n",
    ),
    "strip_non_str": (
        """
"abc".strip(1)
""",
        "Type error",
        "",
    ),
    "find_non_str": (
        """
"abc".find(1)
""",
        "Type error",
        "",
    ),
    "startswith_non_str": (
        """
"abc".startswith(1)
""",
        "Type error",
        "",
    ),
    "str_index_missing": (
        """
print("abc".find("z"))
"abc".index("z")
""",
        "Value error",
        "-1\n",
    ),
    "replace_non_str": (
        """
"abc".replace("a", 1)
""",
        "Type error",
        "",
    ),
    "bytes_find_int_too_big": (
        """
print(b"abc".find(98))
b"abc".find(300)
""",
        "Value error",
        "1\n",
    ),
    "bytes_find_int_negative": (
        """
b"abc".find(-1)
""",
        "Value error",
        "",
    ),
    "bytes_startswith_int": (
        """
b"abc".startswith(97)
""",
        "Type error",
        "",
    ),
    "bytes_replace_int": (
        """
b"abc".replace(97, b"Z")
""",
        "Type error",
        "",
    ),
    "bytes_strip_int": (
        """
b"abc".strip(98)
""",
        "Type error",
        "",
    ),
    "bytes_find_str_needle": (
        """
b"abc".find("b")
""",
        "Type error",
        "",
    ),
    "str_find_bytes_needle": (
        """
"abc".find(b"b")
""",
        "Type error",
        "",
    ),
    "bytes_center_wide_fill": (
        """
b"abc".center(7, b"--")
""",
        "Type error",
        "",
    ),
    "bytes_center_str_fill": (
        """
b"abc".center(7, "-")
""",
        "Type error",
        "",
    ),
    "bytes_join_non_bytes": (
        """
print(b",".join([b"a", b"b"]))
b",".join(["a"])
""",
        "Type error",
        "b'a,b'\n",
    ),
    "bytes_split_empty_sep": (
        """
b"abc".split(b"")
""",
        "Value error",
        "",
    ),
    "bytes_index_missing": (
        """
print(b"abc".find(b"z"))
b"abc".index(b"z")
""",
        "Value error",
        "-1\n",
    ),
    "translate_none_table": (
        """
"abc".translate(None)
""",
        "Type error",
        "",
    ),
    "translate_bad_value": (
        """
"abc".translate({97: [1]})
""",
        "Type error",
        "",
    ),
    "translate_ord_out_of_range": (
        """
print("abc".translate({97: 90}))
"abc".translate({97: 1114112})
""",
        "Value error",
        "Zbc\n",
    ),
    "bytes_translate_short_table": (
        """
b"abc".translate(b"xy")
""",
        "Value error",
        "",
    ),
    "maketrans_unequal_lengths": (
        """
str.maketrans("ab", "xyz")
""",
        "Value error",
        "",
    ),
    "maketrans_single_str_arg": (
        """
str.maketrans("ab")
""",
        "Type error",
        "",
    ),
    "maketrans_multichar_key": (
        """
str.maketrans({"ab": "X"})
""",
        "Value error",
        "",
    ),
    "maketrans_non_str": (
        """
str.maketrans(1, 2)
""",
        "Type error",
        "",
    ),
    "bytes_maketrans_unequal": (
        """
bytes.maketrans(b"ab", b"xyz")
""",
        "Value error",
        "",
    ),
    "bytes_maketrans_str_args": (
        """
bytes.maketrans("ab", "xy")
""",
        "Type error",
        "",
    ),
    "fromhex_odd_length": (
        """
bytes.fromhex("6")
""",
        "Value error",
        "",
    ),
    "fromhex_split_pair": (
        """
bytes.fromhex("6 1")
""",
        "Value error",
        "",
    ),
    "fromhex_non_hex": (
        """
bytes.fromhex("zz")
""",
        "Value error",
        "",
    ),
    "fromkeys_unhashable": (
        """
dict.fromkeys([[1]])
""",
        "Type error",
        "",
    ),
    "fromkeys_non_iterable": (
        """
dict.fromkeys(5)
""",
        "Type error",
        "",
    ),
    "fromkeys_value_keyword": (
        """
dict.fromkeys([1], value=0)
""",
        "Type error",
        "",
    ),
    "unknown_static_method": (
        """
str.nosuchmethod()
""",
        "Attribute error",
        "",
    ),
    "format_mixed_numbering": (
        """
print("{} {}".format(1, 2))
"{0} {}".format(1, 2)
""",
        "Value error",
        "1 2\n",
    ),
    "format_mixed_numbering_reverse": (
        """
"{} {1}".format(1, 2)
""",
        "Value error",
        "",
    ),
    "format_missing_positional": (
        """
"{}".format()
""",
        "Bounds error",
        "",
    ),
    "format_index_out_of_range": (
        """
"{2}".format(1)
""",
        "Bounds error",
        "",
    ),
    "format_missing_keyword": (
        """
"{k}".format()
""",
        "Key error",
        "",
    ),
    "format_unmatched_open": (
        """
"a{".format()
""",
        "Value error",
        "",
    ),
    "format_unmatched_close": (
        """
"a}".format()
""",
        "Value error",
        "",
    ),
    "format_bad_conversion": (
        """
"{!z}".format(1)
""",
        "Value error",
        "",
    ),
    "format_long_conversion": (
        """
"{!rr}".format(1)
""",
        "Value error",
        "",
    ),
    "format_space_name": (
        """
"{ }".format()
""",
        "Key error",
        "",
    ),
    "format_empty_accessor": (
        """
"{0[]}".format([1])
""",
        "Value error",
        "",
    ),
    "format_accessor_out_of_range": (
        """
"{0[2]}".format([1])
""",
        "Bounds error",
        "",
    ),
    "format_accessor_missing_key": (
        """
"{0[z]}".format({"a": 1})
""",
        "Key error",
        "",
    ),
    "format_string_key_on_list": (
        """
"{0[-1]}".format([1, 2])
""",
        "Type error",
        "",
    ),
    "format_attribute_access": (
        """
"{0.real}".format(5)
""",
        "Attribute error",
        "",
    ),
    "format_nested_depth": (
        """
"{:{:{}}}".format(1, 2, 3)
""",
        "Value error",
        "",
    ),
    "format_nested_missing_arg": (
        """
"{:{}}".format(7)
""",
        "Bounds error",
        "",
    ),
    "format_map_positional": (
        """
"{0}".format_map({"0": 1})
""",
        "Value error",
        "",
    ),
    "format_map_auto": (
        """
"{}".format_map({})
""",
        "Value error",
        "",
    ),
    "format_map_non_mapping": (
        """
"{k}".format_map([1])
""",
        "Type error",
        "",
    ),
    "format_map_missing_key": (
        """
"{k}".format_map({})
""",
        "Key error",
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
