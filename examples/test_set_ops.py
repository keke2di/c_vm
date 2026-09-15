def sl(s):
    return sorted(s)

a = {1, 2, 3}
b = {2, 3, 4}
print(sl(a | b), sl(a & b), sl(a - b), sl(a ^ b))
print(sl(b - a), sl({1, 2, 3, 4, 5} - {2, 4}))
print({1, 2} <= {1, 2, 3}, {1, 2} < {1, 2}, {1, 2} < {1, 2, 3})
print({1, 2, 3} >= {1, 2}, {1, 2} > {1, 2}, {1, 2, 3} > {1, 2})
print({1, 5} <= {1, 2, 3}, {1, 2} <= {1, 2}, {1, 2, 3} == {3, 2, 1})
print({1, 2} != {1, 2, 3}, {1, 2} == {1, 2})
s = {1, 2, 3}
s |= {3, 4}
print(sl(s))
s &= {2, 3, 4, 9}
print(sl(s))
s2 = {1, 2, 3, 4}
s2 -= {2}
print(sl(s2))
s2 ^= {1, 9}
print(sl(s2))
fa = frozenset({1, 2})
fb = frozenset({2, 3})
print(sl(fa | fb), type(fa | fb), type(fa | {5}), type({5} | fa))
print(sl(fa & {2, 3}), type(fa - {1}))
print(frozenset({1, 2}) <= {1, 2, 3}, {1} < frozenset({1, 2}), frozenset({1, 2}) == {1, 2})
big = set(range(6))
odd = {1, 3, 5}
print(sl(big - odd), sl(big & odd), sl(big ^ odd))
vowels = set("aeiou")
word = set("education")
print(sl(word & vowels), sl(word - vowels))
