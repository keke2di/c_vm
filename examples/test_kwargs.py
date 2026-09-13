def describe(name, age, city):
    return name + " " + str(age) + " " + city

def sub(a, b):
    return a - b

def total(price, qty, tax):
    subtotal = price * qty
    return subtotal + tax

def apply(fn, x, y):
    return fn(a=y, b=x)

print(describe(name="Ada", age=36, city="London"))
print(describe("Linus", city="Helsinki", age=54))
print(sub(b=2, a=10))
f = sub
print(f(b=1, a=4))
print(apply(sub, 3, 10))
print(total(qty=3, tax=5, price=10))
