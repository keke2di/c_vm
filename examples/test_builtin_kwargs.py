def add(a, b):
    return a + b

print(1, 2, 3, sep=", ")
print("a", "b", sep="", end="|\n")
print("no newline", end="")
print()
print(sep="x")
print("x", "y", sep=None, end=None)
print(end="")
print("flushed", flush=True)
print("file", file=None)
for i, ch in enumerate("abc", start=1):
    print(i, ch, sep=":")
print(list(enumerate(iterable="xy", start=10)))
it = iter([1, 2])
print(next(it), next(it), next(it, "done"))
print(list(zip([1, 2, 3], "abc", strict=True)))
print(list(zip("ab", [1, 2, 3])), list(zip()))
print(list(map(add, [1, 2], [10, 20], strict=True)))
print(list(map(add, [1, 2, 3], [10, 20])))
print(list(map(len, ["", "ab", "abc"])))
print(len)
f = len
print(f("hello"), f(range(7)))
print(str(object=42))
values = [3, 1, 2]
print(values.append(4), values)
print(list(filter(None, [0, 1, "", "x", None, [], [0]])))
