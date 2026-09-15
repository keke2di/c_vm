s = "日本語abc"
print(s.find("abc"), s.rfind("abc"), len(s))
print("café_café".find("é"), "café_café".rfind("é"))
print("café_café".find("é", 4), "café_café".index("é"))

print("abc".find(""), "abc".find("", 1), "abc".find("", 10), "abc".find("", -1))
print("abc".rfind(""), "abc".rfind("", 1), "".find(""), "abc".index(""))
print("abc".find("", 3), "abcabc".find("", 4, 2))
print("abc".count("", 3, 1), "abc".rfind("", 4, 2), "abc".find("", 2, 1))
print("abc".startswith("", 4, 2), "abc".endswith("", 4, 2), "abc".count("z", 3, 1))

print("abcabc".find("b", -3), "abcabc".find("b", 0, -4), "abcabc".find("b", -100))
print("abcabc".find("b", 100), "abcabc".rfind("b", 0, 3), "abc".find("c", 0, -1))
print("abcabc".count("b", -3), "abc".count("", 1, 2), "abc".count(""))

print("aaaa".count("aa"), "aaa".count("aa"), "".count("x"), "aaa".count("a", 1))
print("日x日".count("日"), "abc".count("z"))

print("abc".startswith(("x", "b"), 1), "abc".startswith(""), "abc".startswith("", 10))
print("abc".endswith(("c",), 0, 3), "abc".endswith("b", 0, 2), "abc".startswith(("x", "y")))
print("abc".endswith(""), "abc".startswith("abcd"), "abc".startswith("abc"))
print("日本".startswith("日"), "日本".endswith("本"))

print(repr("aaa".replace("a", "b", 0)), repr("aaa".replace("a", "b", -1)))
print(repr("abc".replace("", "-")), repr("abc".replace("", "-", 2)))
print(repr("".replace("", "x")), repr("aaa".replace("a", "")))
print(repr("abc".replace("z", "y")), repr("日x日".replace("日", "Y")))
print(repr("aXXbXXc".replace("XX", "-")), repr("aaa".replace("aa", "b")))

print("a→b".partition("→"), "ab".partition("-"), "ab".rpartition("-"))
print("a-b-c".partition("-"), "a-b-c".rpartition("-"))
print("".partition("-"), "".rpartition("-"))
