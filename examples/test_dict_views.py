d = {"a": 1, "b": 2}
k, v, it = d.keys(), d.values(), d.items()
print(k, v, it)
print({}.keys(), {}.values(), {}.items(), str(it))
print(type(k), type(v), type(it))
print(len(k), "a" in k, 1 in v, ("a", 1) in it, ("a", 2) in it, "a" in it, ["a", 1] in it, ("a", [1]) in it)
print(k == {"a", "b"}, {"a", "b"} == k, it == {("a", 1), ("b", 2)}, v == v, d.values() == d.values(), k == ["a", "b"], k == d.keys())
print(k == frozenset({"a", "b"}), it == {("a", 1)}, k != {"a"}, d.items() == d.items())
d["c"] = 3
print(k, v, it, len(it))
print(bool({}.keys()), bool(v), list(reversed(k)), list(reversed(v)), list(reversed(it)))
print(list(k), list(v), list(it), dict(it), set(k) == {"a", "b", "c"})
for key, value in d.items():
    print(key, value)
nested = {"x": {"y": 1}.items()}
print(nested, [d.values()])
x, y, z = d.keys()
print(x, y, z)
r = {}
r["self"] = r.values()
print(r)
counts = {"x": 2, "y": 5}
total = 0
for n in counts.values():
    total += n
print(total, [name for name, n in counts.items() if n > 3])
