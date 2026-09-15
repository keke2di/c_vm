lst = [1, 2, 3]
lst[0] += 10
lst[1] *= 5
lst[-1] -= 1
print(lst)
d = {"a": 1, "b": 2}
d["a"] += 100
print(d["a"])
del lst[1]
print(lst)
del d["b"]
print(d)
x = 5
x <<= 2
x |= 1
print(x)
counts = {}
counts["k"] = 0
counts["k"] += 1
counts["k"] += 1
print(counts)
n = 100
del n
n = 7
print(n)
