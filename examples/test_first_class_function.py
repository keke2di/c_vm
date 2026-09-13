def add(a, b):
    return a + b

def apply(fn, x, y):
    return fn(x, y)

def choose(flag):
    if flag:
        return add
    return add

f = add
print(f(7, 8))
print(apply(add, 20, 22))
g = choose(1)
print(g(3, 4))
