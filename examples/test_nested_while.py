total = 0
i = 0

while i < 4:
    j = 0

    while j < 4:
        j += 1

        if j == 2:
            continue

        if j == 4:
            break

        total += 1

    i += 1

print(total)
