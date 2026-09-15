def first(p):
    return p[0]

a = [1, 2, 3]
print(a.append(4), a)
print(a.extend([5, 6]), a)
a.extend("xy")
a.extend(range(2))
print(a)
b = [1, 2, 3]
print(b.insert(1, 99), b)
b.insert(-1, 88)
b.insert(100, 77)
b.insert(-100, 66)
print(b)
c = [10, 20, 30, 40]
print(c.pop(), c)
print(c.pop(0), c)
print(c.pop(-1), c)
d = [1, 2, 3, 2, 1]
print(d.remove(2), d)
d.remove(1.0)
print(d)
print([1, 2, 3].count(2), [1, 1, 1].count(1), [1, True, 1.0].count(1))
print([10, 20, 30, 20].index(20), [10, 20, 30, 20].index(20, 2), [10, 20, 30, 20].index(20, 0, 2))
e = [1, 2, 3]
print(e.reverse(), e)
f = [1, 2, 3]
g = f.copy()
print(g, g == f, g is f)
f.append(4)
print(f, g)
h = [3, 1, 4, 1, 5, 9, 2, 6]
print(h.sort(), h)
h.sort(reverse=True)
print(h)
neg = [-3, 1, -2, 4]
neg.sort(key=abs)
print(neg)
pairs = [(2, "a"), (1, "b"), (2, "c"), (1, "d")]
pairs.sort(key=first)
print(pairs)
stack = []
for i in range(5):
    stack.append(i * i)
print(stack)
while stack:
    stack.pop()
print(stack)
m = [1, 2, 3]
print(m.clear(), m)
words = ["pear", "apple", "fig"]
words.sort()
print(words)
words.sort(key=len, reverse=True)
print(words)
