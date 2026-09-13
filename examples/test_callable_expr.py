def add(a, b):
    return a + b

def sub(a, b):
    return a - b

def choose(flag):
    if flag:
        return add
    return sub

print(choose(1)(3, 4))
print(choose(0)(3, 4))
ops = [add, sub]
print(ops[1](9, 4))
table = {"plus": add}
print(table["plus"](20, 22))
if add:
    print("truthy")
print(add)
print(print)
