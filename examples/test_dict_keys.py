d = {(1, 2): "pair", None: "none", 1: "one", "s": "str", 2.5: "float", b"k": "bytes", (): "empty"}
print(d[(1, 2)], d[None], d[1], d[1.0], d[True], d["s"], d[2.5], d[b"k"], d[()])
d[1.0] = "uno"
d[(1, (2, 3))] = "nested"
print(d)
print({True: 1, 1: 2, 1.0: 3})
print({1: "a", True: "b"}, {0: "z", False: "f", 0.0: "g"})
print(d.get((1, 2)), d.get("missing"), d.get("missing", 0), d.get(1.0, "x"))
print((1, 2) in d, (1, (2, 3)) in d, 3 in d, (1, 2.0) in d)
del d[(1, 2)]
del d[True]
print(d)
s = {(1, 2), (1, 2), (3, 4)}
print(len(s), (1, 2) in s, (5, 6) in s)
t = set()
print(t.add(1), t.add(1.0), t.add(True), len(t))
print(2.0 in {2}, 1 in {1.0}, (1, 2.0) in {(1, 2)})
nested = {}
nested[1, 2] = "tuple key"
print(nested)
counts = {}
for word in ["a", "b", "a", "c", "a"]:
    counts[word] = counts.get(word, 0) + 1
print(counts)
grid = {}
for x in range(2):
    for y in range(2):
        grid[(x, y)] = x * 10 + y
print(grid[(1, 0)], grid)
same = {"k": [1]}
same["k"] = same["k"]
print(same)
