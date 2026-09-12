total = 0

for i in range(4):
    for j in range(4):
        if j == 2:
            continue
        if i == 3:
            break
        total += 1

print(total)
