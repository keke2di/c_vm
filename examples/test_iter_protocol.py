for c in "abc":
    print(c)
for x in (1, 2, 3):
    print(x)
for b in b"xy":
    print(b)
d = {"a": 1, "b": 2}
for k in d:
    print(k)
s = {7}
for v in s:
    print(v)
it = iter([10, 20, 30])
print(next(it))
print(next(it))
print(next(it))
print(next(it, "end"))
nums = [1, 2, 3, 4]
found = 0
for value in nums:
    if value == 3:
        found = value
        break
print(found)
