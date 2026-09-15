def neg(x):
    return -x

def first(p):
    return p[0]

print(sum([1, 2, 3]), sum([]), sum([1.5, 2.5]), sum((1, 2), 10), sum(range(5)), sum([True, True, False]))
print(sum([1, 2], start=100), sum([0.1] * 10), sum([1, 2.0, True]), sum([10 ** 3] * 4))
print(min([3, 1, 2]), max([3, 1, 2]), min(3, 1, 2), max(3, 1, 2), min("bca"), max("bca"))
print(min([], default=9), max([], default=None), min(1, 2, default=0) if False else "skip")
print(min([-3, 1, -2], key=abs), max([-3, 1, -2], key=abs), min(["aa", "b", "ccc"], key=len))
print(min(-3, 1, -2, key=abs), max("hello", "hi", key=len), min([(1, "a"), (1, "b")]), max([1, 1.0, True]))
print(sorted([3, 1, 2]), sorted("bca"), sorted([3, 1, 2], reverse=True), sorted([]))
print(sorted([-3, 1, -2], key=abs), sorted(["bb", "a", "ccc"], key=len, reverse=True))
print(sorted([(1, "a"), (2, "b"), (1, "c"), (2, "d")], key=first))
print(sorted([(1, "a"), (2, "b"), (1, "c"), (2, "d")], key=first, reverse=True))
print(sorted(range(5, 0, -1)), sorted({"b": 1, "a": 2}), sorted({3, 1, 2}))
print(sorted([5, 3, 8, 1], key=neg), type(sorted((3, 1))))
print(any([0, 1, 0]), any([]), all([1, 1]), all([]), any([0, ""]), all([1, 0]))
print(any([x > 2 for x in [1, 2, 3]]), all([x > 0 for x in [1, 2, 3]]), any([0, "x"]), all(["a", "b"]))
print(sum(map(abs, [-1, -2, 3])), max(map(len, ["a", "bbb", "cc"])), sorted(filter(None, [0, 3, 0, 1])))
words = ["pear", "fig", "apple", "kiwi"]
print(sorted(words), sorted(words, key=len), min(words, key=len), max(words, key=len))
print(sum([n * n for n in range(1, 5)]), sorted([3.5, 1, 2.5, 2]))
