x = 5
print("big" if x > 3 else "small")
print("big" if x > 10 else "small")
a = 1 if True else 2
b = 1 if False else 2
print(a, b)
vals = [1, -2, 3, -4]
print([n if n > 0 else -n for n in vals])
y = 0
print(10 // y if y != 0 else "no divide")
label = "even" if x % 2 == 0 else "odd"
print(label)
