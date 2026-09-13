def count_items(rows):
    total = 0
    for row in rows:
        for item in row:
            total += item
    return total

pairs = []
for a in [1, 2]:
    for b in ["x", "y"]:
        pairs.append(str(a) + b)
for p in pairs:
    print(p)

lengths = [len([y for y in x]) for x in [[1, 2], [3], [4, 5, 6]]]
for n in lengths:
    print(n)

sizes = {k: len(k) for k in {"ab": 1, "cde": 2}}
print(sizes["cde"])

letters = {c for c in "abca"}
print(len(letters))

print(count_items([[1, 2], [3, 4], [5]]))
