def summarize(data):
    total = 0
    count = 0

    for key in data:
        values = data[key]

        for value in values:
            if value < 0:
                continue

            total += value
            count += 1

    return total, count


data = {
    "a": [1, 2, -3],
    "b": [4, -5, 6],
}

result = summarize(data)

print(result[0])
print(result[1])

items = {1, 2, 3}
print(2 in items)
print(9 in items)