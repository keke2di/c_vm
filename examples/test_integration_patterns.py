nums = [1, 2, 3, 4, 5]

total = 0
for x in nums:
    if x % 2 == 0:
        continue
    total += x

print(total)

words = ["hello", "world"]
result = ""

for word in words:
    result += word[0]

print(result)

d = {"a": 10, "b": 20, "c": 30}
sum_values = 0

for key in d:
    sum_values += d[key]

print(sum_values)
