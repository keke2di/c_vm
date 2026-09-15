for i in range(3):
    print(i)
for i in range(2, 5):
    print(i)
for i in range(10, 0, -3):
    print(i)
for i in range(5, 0, -1):
    print(i)
print(range(5))
print(range(1, 10, 2))
print(range(0))
print(len(range(0, 10, 2)))
print(len(range(10, 0, -2)))
print(range(3) == range(3))
print(range(0, 3) == range(3))
print(range(5)[2])
print(range(2, 20, 3)[3])
print(range(10)[-1])
print(3 in range(5))
print(7 in range(0, 10, 2))
print(6 in range(0, 10, 2))
r = range(4)
total = 0
for x in r:
    total = total + x
print(total)
a = 2
b = 9
for i in range(a, b, 2):
    print(i)
for i in range(3, 3):
    print("empty")
else:
    print("range-else")
