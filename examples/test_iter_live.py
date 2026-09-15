items = [1, 2, 3]
for x in items:
    if x < 3:
        items.append(x + 10)
print(items)

shrink = [1, 2, 3, 4, 5]
seen = []
for x in shrink:
    seen.append(x)
    if len(shrink) > 2:
        del shrink[-1]
print(seen, shrink)

data = [9]
it = iter(data)
print(next(it), next(it, "end"))
data.append(10)
print(next(it, "still end"))

back = [1, 2, 3, 4]
out = []
for x in reversed(back):
    out.append(x)
    if back:
        del back[0]
print(out, back)

pos = [0]
source = [5, 6, 0, 7]

def produce():
    value = source[pos[0]]
    pos[0] += 1
    return value

print(list(iter(produce, 0)), pos)

text = "héllo wörld"
chars = []
for ch in text:
    chars.append(ch)
print(len(chars), chars[1], chars[7], chars[-1])
r = reversed("añb")
print(next(r), next(r), next(r), next(r, None))
e = enumerate(["x"])
print(next(e), next(e, "stop"))
d = {"a": 1}
it3 = iter(d)
print(next(it3), next(it3, "exhausted"))
d["b"] = 2
print(next(it3, "still exhausted"))
