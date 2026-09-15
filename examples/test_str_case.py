print("ΟΔΟΣ".lower(), "Σ".lower(), "ΑΣΒ".lower())
print("ΑΣ".lower(), "Σ".casefold(), "ς".upper())
print("ΟΔΟΣ".casefold())

print("ß".upper(), "ß".title(), "ß".casefold())
print("ﬃ".upper(), "ﬃ".title(), "ﬃ".casefold())
print("İ".lower(), "ı".upper(), "ẞ".lower(), "ẞ".casefold())

print("hello world".title(), "a1b c'd e".title(), "123abc".title())
print("HELLO".title(), "hELLO wORLD".title(), repr("".title()), "a-b_c".title())
print("ß abc".title(), "x’y".title())
print("hello world".capitalize(), "a1b c'd e".capitalize(), "123abc".capitalize())
print("HELLO".capitalize(), "hELLO wORLD".capitalize(), "ßabc".capitalize())
print("HeLLo".swapcase(), "abc".upper(), "ABC".lower(), "café".upper())

print(repr("ab".center(7, "-")), repr("ab".center(6, "-")), repr("abc".center(2)))
print(repr("ab".ljust(5, ".")), repr("ab".rjust(5, ".")), repr("ab".center(5)))
print(repr("ab".center(0)), repr("ab".center(3)), repr("ab".center(4)))
print(repr("日".center(5, "→")))

print(repr("-5".zfill(6)), repr("+5".zfill(6)), repr("5".zfill(0)))
print(repr("ab".zfill(5)), repr("".zfill(3)), repr("-".zfill(3)))

print(repr("a\tb\tc".expandtabs(4)), repr("a\nb\tc".expandtabs(4)))
print(repr("a\rb\tc".expandtabs(4)), repr("ab\tc".expandtabs()))
print(repr("a\tb".expandtabs(0)), repr("a\tb".expandtabs(1)))
print(repr("a\tb".expandtabs(tabsize=4)))

print("abc".removeprefix("ab"), "abc".removeprefix("z"), "abc".removeprefix(""))
print("abc".removesuffix("bc"), "abc".removesuffix("z"))

words = ["", " ", "abc", "ABC", "Ab", "A1b", "123", "²", "①", "1.5",
         "a b", "_", "1a", "class", "café", "́", "Ab Cd", "aB", "１２"]
for w in words:
    print(repr(w), w.isalpha(), w.isalnum(), w.isdecimal(), w.isdigit(), w.isnumeric(),
          w.isspace(), w.islower(), w.isupper(), w.istitle(), w.isprintable(), w.isidentifier())
