print(list(zip([1, 2], "ab", strict=True)))
print(list(zip([1], [2], [3], strict=True)))
print(list(zip([1, 2], "abc", strict=True)))
print("unreachable")
