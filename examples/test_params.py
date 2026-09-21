def full(a, b=2, *args, key, flag=True, **kwargs):
    return (a, b, args, key, flag, kwargs)

print(full(1, key="k"))
print(full(1, 3, 4, 5, key="k", flag=False, extra=9))
print(full(key="k", a=1))
print(full(1, 2, key="k"))

def pos_only(a, b, /, c, d=4, *, e, f=6):
    return (a, b, c, d, e, f)

print(pos_only(1, 2, 3, e=5))
print(pos_only(1, 2, 3, 40, e=50, f=60))
print(pos_only(1, 2, c=3, e=5))

def defaults(a, b=[], c=None):
    return (a, b, c)

print(defaults(1), defaults(1, [2], 3))

def only_varargs(*values):
    return (values, len(values), sum(values))

print(only_varargs())
print(only_varargs(1, 2, 3))

def only_kwargs(**options):
    return sorted(options.items())

print(only_kwargs(), only_kwargs(z=1, a=2))

def keyword_only(*, name, size=10):
    return (name, size)

print(keyword_only(name="x"), keyword_only(size=1, name="y"))

def many(first, second, third):
    return first * 100 + second * 10 + third

print(many(1, 2, 3), many(1, third=3, second=2))

def evaluate(value=[1, 2]):
    return value

print(evaluate(), evaluate([9]))

base = 5

def scale(factor, offset=base * 2):
    return factor * offset

print(scale(1), scale(2, 1))

def bounded(limit=len([1, 2, 3])):
    return limit

print(bounded(), bounded(10))
