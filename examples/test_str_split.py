print("a,b,c".split(","), "a,b,c".split(",", 1), "a,b,c".rsplit(",", 1))
print("".split(","), "a,,b".split(","), ",a,".split(","), ",".split(","))
print("aXXbXXc".split("XX", 1), "aXXbXXc".rsplit("XX", 1), "abc".split("z"))
print("a,b,c".split(",", 0), "a,b,c".rsplit(",", 0), "a,b,c".split(",", -1))
print("a,b,c".split(sep=","), "a,b,c".split(",", maxsplit=1))

print("  a  b  ".split(), "".split(), "   ".split())
print("a b c".split(maxsplit=1), "a b c".rsplit(maxsplit=1))
print("a\tb\nc".split(), "a\t\t b".split(), "\na\n".split())
print("a b c".split(None, 1), "a b c".rsplit(None, 1))
print(" a ".split(None, 0), " a b ".rsplit(None, 0))
print("a b".split(), "a b".split(), "a\x1cb".split())
print("a→b→c".split("→"), "a→b→c".rsplit("→", 1))

print("a\nb\r\nc\rd".splitlines(), "".splitlines(), "\n".splitlines())
print("a\n\nb".splitlines(), "ab".splitlines(), "a\nb\n".splitlines())
print("a\x0bb\x0cc".splitlines(), "a\x1cb\x1dc\x1ed".splitlines())
print("a\x85b c d".splitlines())
print("a\r\r\nb".splitlines(), "a\n\rb".splitlines())
print("a\n".splitlines(True), "a\nb".splitlines(True), "a\r\nb".splitlines(True))
print("a\rb".splitlines(True), "a b".splitlines(True))
print("a\nb".splitlines(keepends=True))

print(",".join(["a", "b"]), repr(",".join([])), ",".join(["a"]))
print("".join(["a", "b"]), ",".join(("a", "b")), ",".join("abc"))
print("→".join(["a", "b"]), ",".join({"k": "v"}))
print(",".join(["a", "b", "c"]))

print(repr("xxhixx".strip("x")), repr("abcba".strip("ab")), repr("  hi ".strip()))
print(repr("xxhixx".lstrip("x")), repr("xxhixx".rstrip("x")))
print(repr("  hi  ".lstrip()), repr("  hi  ".rstrip()), repr("".strip()))
print(repr("aaa".strip("a")), repr("abc".strip("")), repr("hi".strip(None)))
print(repr("  hi".lstrip()), repr("日x日".strip("日")))
print(repr("abcba".rstrip("ab")))
