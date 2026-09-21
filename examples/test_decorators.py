def trace(fn):
    calls = []

    def wrapper(*args, **kwargs):
        calls.append((args, kwargs))
        return (fn(*args, **kwargs), len(calls))

    return wrapper

@trace
def add(a, b):
    return a + b

print(add(1, 2))
print(add(3, b=4))

def tag(label):
    def decorate(fn):
        def wrapper(*args, **kwargs):
            return (label, fn(*args, **kwargs))
        return wrapper
    return decorate

@tag("first")
@tag("second")
def value():
    return 5

print(value())

def identity(fn):
    return fn

@identity
def plain(x):
    return x * 3

print(plain(4))

def count_calls(fn):
    def wrapper(*args):
        return fn(*args)
    return wrapper

counter = count_calls(lambda v: v + 1)
print(counter(1), counter(10))
