for pair in enumerate(["a", "b", "c"]):
    print(pair)
for pair in enumerate(["x", "y"], 10):
    print(pair)

def dbl(n):
    return n * 2

for v in map(dbl, [1, 2, 3]):
    print(v)

def add(a, b):
    return a + b

for v in map(add, [1, 2, 3], [10, 20, 30]):
    print(v)

def is_even(n):
    return n % 2 == 0

for v in filter(is_even, [1, 2, 3, 4, 5, 6]):
    print(v)

for v in filter(None, [0, 1, "", "x", 0, 5]):
    print(v)

for pair in zip([1, 2, 3], ["a", "b", "c"]):
    print(pair)

for pair in zip([1, 2, 3, 4], ["a", "b"]):
    print(pair)

for c in reversed("abc"):
    print(c)
for x in reversed([1, 2, 3]):
    print(x)
for x in reversed(range(3)):
    print(x)

def sq(n):
    return n * n

for v in map(sq, range(4)):
    print(v)

it = map(dbl, [5, 6])
print(next(it))
print(next(it))
print(next(it, "done"))

def show(x):
    print(x)
    return x

for v in map(dbl, [7, 8]):
    show(v)

total = 0
for v in filter(is_even, range(10)):
    total = total + v
print(total)
