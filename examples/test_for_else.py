for x in [1, 2, 3]:
    print(x)
else:
    print("done")

for x in [1, 2, 3]:
    if x == 2:
        break
    print(x)
else:
    print("not printed")

for x in []:
    print("empty")
else:
    print("empty-else")

total = 0
for x in [10, 20, 30]:
    total = total + x
    if x == 20:
        continue
    print(x)
print(total)
