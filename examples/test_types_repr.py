print(zip, map, filter, enumerate, reversed, object, iter, next)
print(type(zip()), type(map(int, [])), type(filter(None, [])), type(enumerate([])), type(reversed([])))
print(type(iter([])), type(iter(())), type(iter("abc")), type(iter("ä")), type(iter(b"x")), type(iter(range(3))))
d = {1: 2}
print(type(iter({})), type(iter({1})), type(iter(frozenset())), type(iter(d.keys())), type(iter(d.values())), type(iter(d.items())))
print(type(reversed([1])), type(reversed((1,))), type(reversed("ab")), type(reversed(b"ab")), type(reversed(range(3))))
print(type(reversed(d)), type(reversed(d.values())), type(reversed(d.items())))
def one():
    return 1

print(type(iter(one, 0)), type(iter([1, 2], 2)))
print(isinstance(zip([]), zip), isinstance(map(int, []), map), isinstance(iter([]), object), isinstance(1, object))
print(issubclass(bool, int), issubclass(int, object), issubclass(bool, object), issubclass(int, bool))
print(issubclass(type, object), issubclass(object, type), issubclass(zip, object), issubclass(bool, (str, int)))
print(issubclass(int, ()), issubclass(bool, bool), issubclass(bool, type))
print(isinstance(1, (int, (str, float))), isinstance(1, ()), isinstance(True, int), isinstance(1, bool))
print(callable(zip), callable(map), callable(enumerate), callable(reversed), callable(iter), callable(next), callable(object))
o = object()
print(o, type(o), isinstance(o, object), bool(o), o == o, o == object())
print(bool(zip), bool(object), type({1: 2}.keys()), type({1: 2}.values()), type({1: 2}.items()))
