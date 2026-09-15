print(1 < 2 < 3)
print(1 < 3 < 2)
print(3 > 2 > 1)
print(1 <= 1 < 2)
print(1 < 2 > 0)
print(5 < 4 < 3)
print(1 == 1 == 1)
print(1 < 2 <= 2 < 3)

def f():
    print("f")
    return 5

print(1 < f() < 10)
print(20 < f() < 30)
