total = 0

for i in range(4):
    for j in range(3):
        total += 1

    if i == 1:
        continue

    total += 10

print(total)
