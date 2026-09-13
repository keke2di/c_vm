def greet(name, punctuation="!"):
    return "Hello " + name + punctuation

def offset(x, delta=-1):
    return x + delta

def pick(value=None):
    if value:
        return value
    return "empty"

def point(x=0, y=0, label=(1, 2)):
    return len(label) + x + y

print(greet("Ada"))
print(greet("Ada", "?"))
print(greet(punctuation=".", name="Bob"))
print(offset(10))
print(offset(10, 5))
print(offset(10, delta=-3))
print(pick())
print(pick("given"))
print(point())
print(point(y=10))
print(point(1, 2, (1, 2, 3)))
