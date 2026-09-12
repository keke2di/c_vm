def find_first(values):
    for x in values:
        if x > 3:
            return x
    return 0

print(find_first([1, 2, 3, 4, 5]))
print(find_first([1, 2, 3]))
