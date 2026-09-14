def apply(fn, value):
    return fn(value)

p = print
p(5)
p(1, 2, 3)
s = apply(str, 42)
print(s + "!")
x = print("side")
print(x)
print(len([p, apply]))
