def pairs():
    return [("a", 1), ["b", 2], "c3"]

print(tuple(), tuple([1, 2]), tuple("ab"), tuple(range(3)), tuple({"x": 1}))
t = (1, 2)
print(tuple(t) is t, tuple(t) == t)
print(len(set()), set([3, 3, 1]) == {1, 3}, len(set("hello")), 2 in set(range(5)))
print(dict(), dict(a=1, b=2), dict({"k": "v"}), dict(pairs()))
print(dict([(1, "one")], two=2), dict({"a": 1}, a=5), dict(zip("xy", [7, 8])))
print(dict(enumerate("pq")), dict([(1, 1), (1.0, 2), (True, 3)]))
print(list(dict(z=0, y=1)), list(tuple([])))
