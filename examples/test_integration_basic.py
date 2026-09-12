def process(values):
    total = 0

    for value in values:
        if value < 0:
            continue

        total += value

    return total


numbers = [10, -5, 20, -2, 30]
result = process(numbers)

print(result)

message = "integration"
print(message[0])
print(message[3:8])