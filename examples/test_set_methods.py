def sl(s):
    return sorted(s)

a = {1, 2, 3}
b = {2, 3, 4}
print(sl(a.union(b)), sl(a.union([4, 5], {6})), sl(a.union()))
print(sl(a.intersection(b)), sl(a.intersection([2, 3], {3})))
print(sl(a.difference(b)), sl(a.difference([1], [2])))
print(sl(a.symmetric_difference(b)))
print(a.issubset({1, 2, 3, 4}), a.issubset([1, 2, 3]), {1, 2}.issubset(a))
print(a.issuperset({1, 2}), a.issuperset([1, 2, 3, 4]), a.isdisjoint({7, 8}), a.isdisjoint([3]))
s = {1}
print(s.add(2), sl(s))
s.add(2)
print(sl(s), len(s))
r = {1, 2, 3}
print(r.remove(2), sl(r))
print(r.discard(9), r.discard(1), sl(r))
c = {1, 2, 3}
popped = c.pop()
print(popped in {1, 2, 3}, len(c))
cl = {1, 2}
print(cl.clear(), cl, len(cl))
cp = {1, 2, 3}
d = cp.copy()
print(sl(d), d == cp, d is cp)
cp.add(9)
print(sl(cp), sl(d))
u = {1}
print(u.update([2, 3], "4"), sl(u))
iu = {1, 2, 3, 4}
iu.intersection_update({2, 3, 9}, {3, 2})
print(sl(iu))
du = {1, 2, 3, 4}
du.difference_update([1], [2])
print(sl(du))
su = {1, 2, 3}
su.symmetric_difference_update({3, 4, 5})
print(sl(su))
fs = frozenset({1, 2, 3})
print(sl(fs.union({4})), sl(fs.intersection({2, 3})), sl(fs.difference({1})))
print(type(fs.union({4})), fs.issubset({1, 2, 3, 4}), fs.isdisjoint({9}))
print(sl(fs.symmetric_difference({3, 4})), type(fs.copy()))
seen = set()
result = []
for x in [3, 1, 3, 2, 1, 4]:
    if x not in seen:
        seen.add(x)
        result.append(x)
print(result, sl(seen))
