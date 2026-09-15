print(bytes(), bytes(3), bytes(0), bytes([65, 66, 0, 255]), bytes(range(3)), bytes([True, False]))
print(bytes(b"ab"), bytes((1, 2)), bytes({1: 2}), bytes(source=[1]), bytes(iter([1, 2])), bytes(True))
b = b"xy"
print(bytes(b) is b, bytes("hé€\U0001F600", "utf-8"), bytes("abc", encoding="ascii"))
print(bytes("aé\U0001F600b", "ascii", "replace"), "aéb".encode("ascii", "ignore"))
print("aé€\U0001F600".encode("ascii", "backslashreplace"), "aé\U0001F600".encode("ascii", "xmlcharrefreplace"))
print("éÿ".encode("latin-1"), "€".encode("latin-1", "replace"), "Ā\U00010000".encode("latin-1", "backslashreplace"))
print("é".encode(), "x".encode(encoding="utf-8", errors="strict"), "".encode("utf-8-sig"), "x".encode("utf-8-sig"))
print(str(b"h\xc3\xa9", "utf-8"), str(b"abc", encoding="ascii"), str(b"\xc3\xa9", errors="strict"), repr(str(encoding="utf-8")))
print(repr(str(b"a\xffb", "utf-8", "replace")), str(b"a\xffb", "utf-8", "ignore"), b"a\xffb".decode("utf-8", "backslashreplace"))
print(b"\xe9\x41\xff".decode("latin-1"), repr(b"a\x80\xffb".decode("ascii", "replace")), str(b"\xc3\xa9"))
for raw in [b"\xed\xa0\x80", b"\xf4\x90\x80\x80", b"\xc0\x80", b"\xe0\x80\x80", b"\xe2\x82", b"\xf0\x9f\x98", b"\xf0\x9fA", b"\x80\x80", b"\xf5\x80", b"\xf0\x8f\xbf\xbf", b"\xf0\x9fA\xe2\x82"]:
    print(raw, len(raw.decode("utf-8", "replace")), raw.decode("utf-8", "backslashreplace"), repr(raw.decode("utf-8", "ignore")))
print(len(b"\xe0\xa0\x80".decode()), len(b"\xed\x9f\xbf".decode()), len(b"\xf0\x90\x80\x80".decode()), b"\xf0\x9f\x98\x80".decode() == "\U0001F600")
print(len(b"\xef\xbb\xbfx".decode("utf-8")), b"\xef\xbb\xbfx".decode("utf-8-sig"), repr(b"\xef\xbb\xbf".decode("utf-8-sig")), len(b"\xef\xbb".decode("utf-8-sig", "replace")))
print(len(b"\xef\xbb\xbf\xef\xbb\xbfx".decode("utf-8-sig")), repr(b"a\x00b".decode()), b"x".decode(encoding="ascii", errors="ignore"))
for name in ["UTF-8", "utf8", "U8", "utf_8", "UTF", "cp65001", "utf-8_sig", "latin-1", "Latin1", "ISO-8859-1", "iso8859_1", "8859", "cp819", "L1", "latin", "US-ASCII", "646", "us", " utf-8", "utf--8", "-utf-8-", "ANSI_X3.4-1968", "iso646-us", "UTF 8"]:
    print(repr(name), b"A\xc3\xa9".decode(name, "replace"))
print(repr(str(b"", "bogus")), repr(b"".decode("bogus")), "x".encode("ascii", "bogus"), b"x".decode("utf-8", "bogus"))
text = "naïve café"
data = text.encode("utf-8")
print(data, len(data), data.decode("utf-8") == text, text.encode("latin-1").decode("latin-1") == text)
