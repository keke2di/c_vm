items = [1, 2, 3, 4, 5]
items[1:3] = [20, 30, 40]
print(items)

items[0:0] = [0]
print(items)

items[2:2] = []
print(items)

values = [1, 2, 3, 4, 5, 6]
values[::2] = [10, 30, 50]
print(values)

values[::-1] = [1, 2, 3]
print(values)

words = ["a", "b", "c", "d"]
words[1:3] = "xy"
print(words)

nums = [1, 2, 3, 4]
nums[1:2] = (7, 8, 9)
print(nums)

del nums[1:3]
print(nums)

del nums[-1:]
print(nums)

other = [0, 1, 2, 3, 4, 5, 6, 7]
del other[::3]
print(other)

more = [0, 1, 2, 3, 4, 5]
del more[4:1:-1]
print(more)

plain = [1, 2, 3]
plain[:] = [9]
print(plain)

aug = [1, 2, 3, 4]
aug[1:3] += [10]
print(aug)

counter = [0]
counter[0:1] = [counter[0] + 1]
print(counter)

chained = [1, 2, 3]
first = chained[0:2] = [7, 8]
print(first, chained)

nested = [[1, 2], [3, 4]]
nested[0][0:1] = [9]
print(nested)

empty = []
empty[0:5] = [1, 2]
print(empty)

calls = [0]
data = [1, 2, 3, 4]

def target():
    calls[0] = calls[0] + 1
    return data

target()[0:2] += [9]
print(calls[0], data)

markers = [0]

def start():
    markers[0] = markers[0] + 1
    return 1

def stop():
    markers[0] = markers[0] + 10
    return 3

data[start():stop()] = [7]
print(markers[0], data)

snippet = [1, 2, 3]
snippet[1:3] = [20, 30]
snippet[::2] = [1, 2]
del snippet[1:3]
del snippet[::2]
print(snippet)
