d = {"a": 1, "b": 2}
print("before")
for k in d:
    print(k)
    d["c" + k] = 3
print("unreachable")
