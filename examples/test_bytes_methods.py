print(b"abc".find(b"b"), b"abc".rfind(b"b"), b"abc".find(b"z"))
print("日abc".encode().find(b"abc"), len("日abc".encode()))
print(b"abc".find(98), b"abc".count(97), b"abc".index(98), b"abc".rindex(98))
print(b"abcabc".find(b"b", -3), b"abcabc".find(b"b", 0, -4), b"abc".find(b"", 10))
print(b"abc".count(b""), b"aaaa".count(b"aa"), b"abc".count(b"z"))

print(b"abc".startswith(b"ab"), b"abc".startswith((b"z", b"a")), b"abc".endswith(b"bc"))
print(b"abc".startswith(b"", 10), b"abc".endswith(b"b", 0, 2))

print(b"abc".replace(b"a", b"Z"), b"aaa".replace(b"a", b"b", 2))
print(b"abc".replace(b"", b"-"), b"aaa".replace(b"a", b""))
print(b"abc".partition(b"b"), b"ab".partition(b"-"), b"ab".rpartition(b"-"))
print(b"a-b-c".partition(b"-"), b"a-b-c".rpartition(b"-"))

print(b"a,b,c".split(b","), b"a,b,c".split(b",", 1), b"a,b,c".rsplit(b",", 1))
print(b"".split(b","), b"a,,b".split(b","), b"abc".split(b"z"))
print(b"  a  b ".split(), b"".split(), b"   ".split())
print(b"a\tb\nc".split(), b" a ".split(None, 0), b" a b ".rsplit(None, 0))
print(b"a b c".split(maxsplit=1), b"a,b".split(sep=b","))

print(b"a\nb\r\nc\rd".splitlines(), b"".splitlines(), b"\n".splitlines())
print(b"a\x0bb\x0cc".splitlines(), b"a\x1cb".splitlines())
print(b"a\r\nb".splitlines(True), b"a\nb".splitlines(True), b"a\rb".splitlines(True))
print(b"a\r\r\nb".splitlines())

print(b",".join([b"a", b"b"]), b"".join([b"a", b"b"]), b",".join([]))
print(b",".join((b"a", b"b")))

print(b"xxhixx".strip(b"x"), b"  hi  ".strip(), b"  a  b ".strip())
print(b"xxhixx".lstrip(b"x"), b"xxhixx".rstrip(b"x"), b"abc".strip(b""))
print(b"\t\n hi \r\n".strip())

print(b"abc".upper(), b"ABC".lower(), b"\xe9abc".upper())
print(b"hello world".title(), b"a1b c".title(), b"\xe9abc".title())
print(b"hELLO".capitalize(), b"HeLLo".swapcase())

print(b"abc".center(7, b"-"), b"abc".center(8, b"-"), b"abc".center(2))
print(b"ab".ljust(5, b"."), b"ab".rjust(5, b"."), b"ab".center(5))
print(b"-5".zfill(6), b"+5".zfill(6), b"ab".zfill(5), b"".zfill(3))
print(b"a\tb\tc".expandtabs(4), b"a\nb\tc".expandtabs(4), b"ab\tc".expandtabs())
print(b"a\tb".expandtabs(0), b"a\tb".expandtabs(tabsize=4))
print(b"abc".removeprefix(b"ab"), b"abc".removesuffix(b"bc"), b"abc".removeprefix(b"z"))

print(b"abc".hex(), b"abc".hex("-"), b"abcd".hex("-", 2), b"abcd".hex("-", -2))
print(b"".hex())

items = [b"", b" ", b"abc", b"ABC", b"Ab", b"A1b", b"123", b"\xe9", b"a b", b"Ab Cd"]
for it in items:
    print(it, it.isalpha(), it.isdigit(), it.isalnum(), it.isspace(),
          it.islower(), it.isupper(), it.istitle(), it.isascii())

print("abc".isascii(), "café".isascii(), "".isascii(), "日".isascii())
