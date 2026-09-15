d = {"a": 1, "b": 2, "c": 3}
print(d.pop("a"), d)
print(d.pop("x", 99), d)
print(d.pop("y", None))
e = {"a": 1, "b": 2}
print(e.popitem(), e)
print(e.popitem(), e)
f = {"a": 1}
print(f.setdefault("a", 5), f)
print(f.setdefault("b", 9), f)
print(f.setdefault("c"), f)
g = {"a": 1}
print(g.update({"b": 2, "a": 9}), g)
g.update([("c", 3), ("d", 4)])
g.update(e=5, f=6)
g.update({"g": 7}, g=99, h=8)
print(g)
h = {"a": 1, "b": 2}
print(h.clear(), h)
i = {"x": 1, "y": 2}
j = i.copy()
print(j, j == i, j is i)
i["z"] = 3
print(i, j)
print({True: "t", 2: "b"}.pop(1.0))
print({1: "one", 2: "two"} | {2: "TWO", 3: "three"})
u = {"a": 1}
u |= {"b": 2, "a": 9}
print(u)
counts = {}
for ch in "abracadabra":
    counts[ch] = counts.get(ch, 0) + 1
print(counts)
groups = {}
for n in range(10):
    groups.setdefault(n % 3, []).append(n)
print(groups)
merged = {"base": 0}
merged.update({"x": 1}, y=2, z=3)
print(merged, sorted(merged.keys()), sorted(merged.values()))
