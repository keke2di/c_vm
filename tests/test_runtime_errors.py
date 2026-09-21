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
        "TypeError",
        "1\n",
    ),
    "call_string": (
        """
x = "abc"
x(1)
""",
        "TypeError",
        "",
    ),
    "too_many_args": (
        """
def f(a):
    return a

f(1, 2)
""",
        "TypeError",
        "",
    ),
    "too_few_args": (
        """
def f(a, b):
    return a

f(1)
""",
        "TypeError",
        "",
    ),
    "builtin_bad_arity": (
        """
str(1, 2)
""",
        "TypeError",
        "",
    ),
    "undefined_function": (
        """
missing(1)
""",
        "NameError",
        "",
    ),
    "unknown_keyword": (
        """
def f(a):
    return a

f(b=1)
""",
        "TypeError",
        "",
    ),
    "duplicate_argument": (
        """
def f(a, b):
    return a

f(1, a=2)
""",
        "TypeError",
        "",
    ),
    "missing_required_argument": (
        """
def f(a, b=2):
    return a

f(b=3)
""",
        "TypeError",
        "",
    ),
    "too_many_positional_with_defaults": (
        """
def f(a, b=2):
    return a

f(1, 2, 3)
""",
        "TypeError",
        "",
    ),
    "unknown_builtin_keyword": (
        """
print(1, foo=2)
""",
        "TypeError",
        "",
    ),
    "keyword_call_non_callable": (
        """
x = 5
x(a=1)
""",
        "TypeError",
        "",
    ),
    "missing_dict_key": (
        """
d = {"a": 1}
print(d["a"])
d["b"]
""",
        "KeyError",
        "1\n",
    ),
    "delete_missing_dict_key": (
        """
d = {}
del d["x"]
""",
        "KeyError",
        "",
    ),
    "unhashable_dict_key": (
        """
d = {}
d[[1]] = 2
""",
        "TypeError",
        "",
    ),
    "unhashable_set_item": (
        """
s = {1}
s.add({})
""",
        "TypeError",
        "",
    ),
    "unhashable_membership": (
        """
print([1] in [[1]])
[1] in {1: 2}
""",
        "TypeError",
        "True\n",
    ),
    "unknown_method": (
        """
x = [1]
x.nope()
""",
        "AttributeError",
        "",
    ),
    "method_keyword_rejected": (
        """
x = []
x.append(item=1)
""",
        "TypeError",
        "",
    ),
    "invalid_int_literal": (
        """
print(int("12"))
int("12a")
""",
        "ValueError",
        "12\n",
    ),
    "float_to_int_overflow": (
        """
int(float("inf"))
""",
        "OverflowError",
        "",
    ),
    "dict_changed_during_iteration": (
        """
d = {"a": 1}
for k in d:
    d["b"] = 2
""",
        "RuntimeError",
        "",
    ),
    "unknown_encoding": (
        """
"x".encode("bogus")
""",
        "LookupError",
        "",
    ),
    "unknown_error_handler_used": (
        """
print("x".encode("ascii", "bogus"))
"\\u00e9".encode("ascii", "bogus")
""",
        "LookupError",
        "b'x'\n",
    ),
    "invalid_utf8": (
        """
b"\\xff".decode()
""",
        "UnicodeError",
        "",
    ),
    "decode_with_xmlcharrefreplace": (
        """
b"\\xff".decode("utf-8", "xmlcharrefreplace")
""",
        "TypeError",
        "",
    ),
    "bytes_from_str_without_encoding": (
        """
bytes("x")
""",
        "TypeError",
        "",
    ),
    "bytes_item_out_of_range": (
        """
bytes([1, 256])
""",
        "ValueError",
        "",
    ),
    "decode_str": (
        """
str("x", "utf-8")
""",
        "TypeError",
        "",
    ),
    "unhashable_dict_view": (
        """
{{}.keys(): 1}
""",
        "TypeError",
        "",
    ),
    "frozenset_has_no_add": (
        """
frozenset().add(1)
""",
        "AttributeError",
        "",
    ),
    "list_pop_empty": (
        """
[].pop()
""",
        "IndexError",
        "",
    ),
    "list_remove_missing": (
        """
print([1, 2].remove(2))
[1, 2].remove(9)
""",
        "ValueError",
        "None\n",
    ),
    "list_index_missing": (
        """
[1, 2, 3].index(9)
""",
        "ValueError",
        "",
    ),
    "set_remove_missing": (
        """
{1, 2}.remove(9)
""",
        "KeyError",
        "",
    ),
    "set_pop_empty": (
        """
set().pop()
""",
        "KeyError",
        "",
    ),
    "dict_popitem_empty": (
        """
{}.popitem()
""",
        "KeyError",
        "",
    ),
    "set_op_non_set": (
        """
print({1, 2} | {3})
{1} & [2]
""",
        "TypeError",
        "{1, 2, 3}\n",
    ),
    "dict_or_non_dict": (
        """
{"a": 1} | [1]
""",
        "TypeError",
        "",
    ),
    "split_empty_sep": (
        """
print("a,b".split(","))
"abc".split("")
""",
        "ValueError",
        "['a', 'b']\n",
    ),
    "rsplit_empty_sep": (
        """
"abc".rsplit("")
""",
        "ValueError",
        "",
    ),
    "partition_empty_sep": (
        """
"abc".partition("")
""",
        "ValueError",
        "",
    ),
    "join_non_str": (
        """
print(",".join(["a", "b"]))
",".join(["a", 1])
""",
        "TypeError",
        "a,b\n",
    ),
    "strip_non_str": (
        """
"abc".strip(1)
""",
        "TypeError",
        "",
    ),
    "find_non_str": (
        """
"abc".find(1)
""",
        "TypeError",
        "",
    ),
    "startswith_non_str": (
        """
"abc".startswith(1)
""",
        "TypeError",
        "",
    ),
    "str_index_missing": (
        """
print("abc".find("z"))
"abc".index("z")
""",
        "ValueError",
        "-1\n",
    ),
    "replace_non_str": (
        """
"abc".replace("a", 1)
""",
        "TypeError",
        "",
    ),
    "bytes_find_int_too_big": (
        """
print(b"abc".find(98))
b"abc".find(300)
""",
        "ValueError",
        "1\n",
    ),
    "bytes_find_int_negative": (
        """
b"abc".find(-1)
""",
        "ValueError",
        "",
    ),
    "bytes_startswith_int": (
        """
b"abc".startswith(97)
""",
        "TypeError",
        "",
    ),
    "bytes_replace_int": (
        """
b"abc".replace(97, b"Z")
""",
        "TypeError",
        "",
    ),
    "bytes_strip_int": (
        """
b"abc".strip(98)
""",
        "TypeError",
        "",
    ),
    "bytes_find_str_needle": (
        """
b"abc".find("b")
""",
        "TypeError",
        "",
    ),
    "str_find_bytes_needle": (
        """
"abc".find(b"b")
""",
        "TypeError",
        "",
    ),
    "bytes_center_wide_fill": (
        """
b"abc".center(7, b"--")
""",
        "TypeError",
        "",
    ),
    "bytes_center_str_fill": (
        """
b"abc".center(7, "-")
""",
        "TypeError",
        "",
    ),
    "bytes_join_non_bytes": (
        """
print(b",".join([b"a", b"b"]))
b",".join(["a"])
""",
        "TypeError",
        "b'a,b'\n",
    ),
    "bytes_split_empty_sep": (
        """
b"abc".split(b"")
""",
        "ValueError",
        "",
    ),
    "bytes_index_missing": (
        """
print(b"abc".find(b"z"))
b"abc".index(b"z")
""",
        "ValueError",
        "-1\n",
    ),
    "translate_none_table": (
        """
"abc".translate(None)
""",
        "TypeError",
        "",
    ),
    "translate_bad_value": (
        """
"abc".translate({97: [1]})
""",
        "TypeError",
        "",
    ),
    "translate_ord_out_of_range": (
        """
print("abc".translate({97: 90}))
"abc".translate({97: 1114112})
""",
        "ValueError",
        "Zbc\n",
    ),
    "bytes_translate_short_table": (
        """
b"abc".translate(b"xy")
""",
        "ValueError",
        "",
    ),
    "maketrans_unequal_lengths": (
        """
str.maketrans("ab", "xyz")
""",
        "ValueError",
        "",
    ),
    "maketrans_single_str_arg": (
        """
str.maketrans("ab")
""",
        "TypeError",
        "",
    ),
    "maketrans_multichar_key": (
        """
str.maketrans({"ab": "X"})
""",
        "ValueError",
        "",
    ),
    "maketrans_non_str": (
        """
str.maketrans(1, 2)
""",
        "TypeError",
        "",
    ),
    "bytes_maketrans_unequal": (
        """
bytes.maketrans(b"ab", b"xyz")
""",
        "ValueError",
        "",
    ),
    "bytes_maketrans_str_args": (
        """
bytes.maketrans("ab", "xy")
""",
        "TypeError",
        "",
    ),
    "fromhex_odd_length": (
        """
bytes.fromhex("6")
""",
        "ValueError",
        "",
    ),
    "fromhex_split_pair": (
        """
bytes.fromhex("6 1")
""",
        "ValueError",
        "",
    ),
    "fromhex_non_hex": (
        """
bytes.fromhex("zz")
""",
        "ValueError",
        "",
    ),
    "fromkeys_unhashable": (
        """
dict.fromkeys([[1]])
""",
        "TypeError",
        "",
    ),
    "fromkeys_non_iterable": (
        """
dict.fromkeys(5)
""",
        "TypeError",
        "",
    ),
    "fromkeys_value_keyword": (
        """
dict.fromkeys([1], value=0)
""",
        "TypeError",
        "",
    ),
    "unknown_static_method": (
        """
str.nosuchmethod()
""",
        "AttributeError",
        "",
    ),
    "format_mixed_numbering": (
        """
print("{} {}".format(1, 2))
"{0} {}".format(1, 2)
""",
        "ValueError",
        "1 2\n",
    ),
    "format_mixed_numbering_reverse": (
        """
"{} {1}".format(1, 2)
""",
        "ValueError",
        "",
    ),
    "format_missing_positional": (
        """
"{}".format()
""",
        "IndexError",
        "",
    ),
    "format_index_out_of_range": (
        """
"{2}".format(1)
""",
        "IndexError",
        "",
    ),
    "format_missing_keyword": (
        """
"{k}".format()
""",
        "KeyError",
        "",
    ),
    "format_unmatched_open": (
        """
"a{".format()
""",
        "ValueError",
        "",
    ),
    "format_unmatched_close": (
        """
"a}".format()
""",
        "ValueError",
        "",
    ),
    "format_bad_conversion": (
        """
"{!z}".format(1)
""",
        "ValueError",
        "",
    ),
    "format_long_conversion": (
        """
"{!rr}".format(1)
""",
        "ValueError",
        "",
    ),
    "format_space_name": (
        """
"{ }".format()
""",
        "KeyError",
        "",
    ),
    "format_empty_accessor": (
        """
"{0[]}".format([1])
""",
        "ValueError",
        "",
    ),
    "format_accessor_out_of_range": (
        """
"{0[2]}".format([1])
""",
        "IndexError",
        "",
    ),
    "format_accessor_missing_key": (
        """
"{0[z]}".format({"a": 1})
""",
        "KeyError",
        "",
    ),
    "format_string_key_on_list": (
        """
"{0[-1]}".format([1, 2])
""",
        "TypeError",
        "",
    ),
    "format_attribute_access": (
        """
"{0.real}".format(5)
""",
        "AttributeError",
        "",
    ),
    "format_nested_depth": (
        """
"{:{:{}}}".format(1, 2, 3)
""",
        "ValueError",
        "",
    ),
    "format_nested_missing_arg": (
        """
"{:{}}".format(7)
""",
        "IndexError",
        "",
    ),
    "format_map_positional": (
        """
"{0}".format_map({"0": 1})
""",
        "ValueError",
        "",
    ),
    "format_map_auto": (
        """
"{}".format_map({})
""",
        "ValueError",
        "",
    ),
    "format_map_non_mapping": (
        """
"{k}".format_map([1])
""",
        "TypeError",
        "",
    ),
    "format_map_missing_key": (
        """
"{k}".format_map({})
""",
        "KeyError",
        "",
    ),
    "hash_list": (
        """
hash([1, 2])
""",
        "TypeError",
        "",
    ),
    "hash_dict": (
        """
print("before")
hash({})
""",
        "TypeError",
        "before\n",
    ),
    "hash_set": (
        """
hash({1})
""",
        "TypeError",
        "",
    ),
    "hash_tuple_with_list": (
        """
hash((1, [2]))
""",
        "TypeError",
        "",
    ),
    "hash_dict_view": (
        """
d = {1: 2}
hash(d.keys())
""",
        "TypeError",
        "",
    ),
    "hash_no_args": (
        """
hash()
""",
        "TypeError",
        "",
    ),
    "hash_two_args": (
        """
hash(1, 2)
""",
        "TypeError",
        "",
    ),
    "hash_keyword": (
        """
hash(x=1)
""",
        "TypeError",
        "",
    ),
    "slice_assign_string": (
        """
s = "abc"
s[0:1] = "x"
""",
        "TypeError",
        "",
    ),
    "slice_assign_tuple": (
        """
t = (1, 2)
t[0:1] = [9]
""",
        "TypeError",
        "",
    ),
    "slice_assign_dict": (
        """
d = {}
d[0:1] = [9]
""",
        "TypeError",
        "",
    ),
    "slice_delete_string": (
        """
s = "abc"
del s[0:1]
""",
        "TypeError",
        "",
    ),
    "slice_store_step_zero": (
        """
x = [1, 2, 3]
x[0:2:0] = [9]
""",
        "ValueError",
        "",
    ),
    "slice_delete_step_zero": (
        """
x = [1, 2, 3]
del x[0:2:0]
""",
        "ValueError",
        "",
    ),
    "slice_extended_length_mismatch": (
        """
x = [1, 2, 3]
x[::2] = [1, 2, 3]
""",
        "ValueError",
        "",
    ),
    "slice_string_bound": (
        """
x = [1, 2, 3]
x["a":1] = [9]
""",
        "TypeError",
        "",
    ),
    "issubclass_not_a_type": (
        """
issubclass(1, int)
""",
        "TypeError",
        "",
    ),
    "issubclass_bad_classinfo": (
        """
issubclass(int, 1)
""",
        "TypeError",
        "",
    ),
    "isinstance_bad_classinfo": (
        """
isinstance(1, "int")
""",
        "TypeError",
        "",
    ),
    "isinstance_tuple_entry": (
        """
print(isinstance(1, (2, int)))
""",
        "TypeError",
        "",
    ),
    "object_len": (
        """
len(object())
""",
        "TypeError",
        "",
    ),
    "object_iteration": (
        """
for x in object():
    print(x)
""",
        "TypeError",
        "",
    ),
    "object_index": (
        """
object()[0]
""",
        "TypeError",
        "",
    ),
    "reversed_set": (
        """
reversed({1, 2})
""",
        "TypeError",
        "",
    ),
    "unhashable_set_contains": (
        """
print([1] in {2})
""",
        "TypeError",
        "",
    ),
    "too_many_positional": (
        """
def f(a):
    return a

print(f(1, 2))
""",
        "TypeError",
        "",
    ),
    "unexpected_keyword": (
        """
def f(a):
    return a

print(f(1, other=2))
""",
        "TypeError",
        "",
    ),
    "duplicate_argument": (
        """
def f(a):
    return a

print(f(1, a=2))
""",
        "TypeError",
        "",
    ),
    "missing_argument": (
        """
def f(a, b):
    return a

print(f(1))
""",
        "TypeError",
        "",
    ),
    "missing_keyword_only": (
        """
def f(a, *, flag):
    return a

print(f(1))
""",
        "TypeError",
        "",
    ),
    "positional_only_as_keyword": (
        """
def f(a, /):
    return a

print(f(a=1))
""",
        "TypeError",
        "",
    ),
    "star_argument_not_iterable": (
        """
def f(*values):
    return values

print(f(*1))
""",
        "TypeError",
        "",
    ),
    "double_star_not_mapping": (
        """
def f(**options):
    return options

print(f(**[]))
""",
        "TypeError",
        "",
    ),
    "double_star_duplicate": (
        """
def f(**options):
    return options

print(f(**{"a": 1}, **{"a": 2}))
""",
        "TypeError",
        "",
    ),
    "unbound_cell_read": (
        """
def outer():
    def inner():
        return total

    inner()
    total = 5
    return total

print(outer())
""",
        "UnboundLocalError",
        "",
    ),
    "call_before_definition": (
        """
print(later())

def later():
    return 1
""",
        "NameError",
        "",
    ),
    "uncaught_raise": (
        """
print("before")
raise ValueError("boom")
""",
        "ValueError",
        "before\n",
    ),
    "assertion_failure": (
        """
value = 1
assert value > 5, "too small"
""",
        "AssertionError",
        "",
    ),
    "bare_reraise_outside_handler": (
        """
print("start")
raise
""",
        "RuntimeError",
        "start\n",
    ),
    "raise_non_exception": (
        """
raise 42
""",
        "TypeError",
        "",
    ),
    "unhandled_except_clause": (
        """
try:
    raise ValueError("kept")
except KeyError:
    print("wrong")
""",
        "ValueError",
        "",
    ),
    "handler_runs_finally": (
        """
try:
    try:
        raise ValueError("x")
    finally:
        print("cleanup")
except ValueError:
    raise KeyError("second")
""",
        "KeyError",
        "cleanup\n",
    ),
    "traceback_reports_line": (
        """
value = 1
print("one")
value[0]
""",
        "line 3",
        "one\n",
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
