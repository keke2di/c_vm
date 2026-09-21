x = 100
doubled = [x * 2 for x in range(3)]
print(doubled, x)

names = ["ab", "cde"]
print([[ch for ch in name] for name in names], names)
print([n for n in names for ch in n])
print([a + b for a, b in [(1, 2), (3, 4)]])
print(sorted([k % 3 for k in range(10)]))

def f():
    x = 5
    return ([x + i for i in range(3)], x)

print(f())

def h():
    t = "outer"
    [t for t in range(3)]
    return t

print(h())

def g():
    total = 0
    for v in [len([w for w in range(3)]) for _ in range(2)]:
        total = total + v
    return total

print(g())

def keep():
    seen = []
    for pair in [(1, 2), (3, 4)]:
        seen.append([pair[0] + pair[1] for pair in [pair]])
    return seen

print(keep())
print("before probe")
print([c for c in "xyz"], c)
