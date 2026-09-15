a, b = 1, 2
print(a, b)
a, b = b, a
print(a, b)
x, y, z = [10, 20, 30]
print(x, y, z)
(p, q), r = (1, 2), 3
print(p, q, r)
first, *rest = [1, 2, 3, 4]
print(first, rest)
head, *mid, tail = [1, 2, 3, 4, 5]
print(head, mid, tail)
*init, last = "abcd"
print(init, last)
m = n = 7
print(m, n)
c1 = c2 = c3 = 0
print(c1, c2, c3)
for i, ch in enumerate(["a", "b"]):
    print(i, ch)
for k, v in zip([1, 2], ["x", "y"]):
    print(k, v)
