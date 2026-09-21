double = lambda x: x * 2
print(double(4), double(-1))

add = lambda a, b=10: a + b
print(add(1), add(1, 2))

def make_power(exp):
    return lambda base: base ** exp

square = make_power(2)
cube = make_power(3)
print(square(5), cube(3), make_power(0)(9))

def apply_twice(fn, value):
    return fn(fn(value))

print(apply_twice(lambda v: v + 3, 1))

pairs = [(2, "b"), (1, "a"), (3, "c")]
print(sorted(pairs, key=lambda pair: pair[0]))
print(sorted(pairs, key=lambda pair: pair[1]))

adders = [lambda v, n=n: v + n for n in range(3)]
print([fn(10) for fn in adders])

print((lambda: 42)())
print((lambda *args, **kwargs: (args, kwargs))(1, 2, flag=True))
