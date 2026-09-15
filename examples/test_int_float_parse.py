for text in ["42", " 42\n", "+7", "-0", "4_2", "1_000_000", "000", "010", "\t-9\x0b"]:
    print(repr(text), int(text))
print(int(" 42　"), float("  3.75  "), int("\x855\xa0"), float("\x85-1.5 "))
for text, base in [("ff", 16), ("0xff", 16), ("0x_ff", 0), ("0o17", 0), ("0b_101", 0), ("-0b11", 0), ("z", 36), ("Zz", 36), ("0b1", 16), ("777", 8), ("0_0", 0), ("  12  ", 3), ("0X_F", 0)]:
    print(repr(text), base, int(text, base))
print(int("10", base=2), int(b"42"), int(b" 0x1f ", 16), int(2.9), int(-2.9), int(True), int(), int(-0.5))
print(int("9223372036854775807"), int("-9223372036854775808"), int(-9.2e18))
for text in ["1.5", " -2.25 ", "1_0.5", "1e5", "1E-3", ".5", "5.", "-.5", "inf", "-Infinity", "+iNf", "nan", "-nan", "1e500", "-1e500", "1.5e-400", "0_0.0_0", "1.e5", "1.5E+3", "3.14159265358979"]:
    print(repr(text), float(text))
print(float(b"2.5"), float(3), float(True), float(), float("  -0.0"), float(-7))
print(bool(), bool(0), bool("x"), bool([]), bool(range(0)))
print(list(), list("héllo"), list(range(3)), list((1, 2)), list({"a": 1, "b": 2}), list({3}))
print(str(), str(object="text"), str(b"raw"), str(None), str([1, "a"]), str(1.5))
print(type(range(3)) is range, list(reversed(range(4))), list(reversed("abç")))
