x = 0
while x < 3:
    print(x)
    x = x + 1
else:
    print("while-done")

n = 0
while True:
    n = n + 1
    if n == 3:
        break
    print(n)
else:
    print("unreached")
print(n)

while False:
    print("never")
else:
    print("false-else")
