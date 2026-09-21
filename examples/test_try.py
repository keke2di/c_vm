def risky(value):
    if value < 0:
        raise ValueError("negative", value)
    return value * 2

for value in [2, -1, 3]:
    try:
        print("try", risky(value))
    except ValueError as error:
        print("caught", error)
    else:
        print("else")
    finally:
        print("finally")

try:
    print([1, 2][9])
except IndexError:
    print("index")
except LookupError:
    print("lookup")

try:
    print({}["missing"])
except LookupError as error:
    print("lookup key", error)

def cleanup(log):
    try:
        log.append("body")
        return "early"
    finally:
        log.append("finally")

log = []
print(cleanup(log), log)

def loop_finally():
    seen = []
    for index in range(4):
        try:
            if index == 1:
                continue
            if index == 3:
                break
            seen.append(index)
        finally:
            seen.append("f" + str(index))
    return seen

print(loop_finally())

def nested():
    order = []
    try:
        try:
            raise RuntimeError("inner")
        finally:
            order.append("inner-finally")
    except RuntimeError:
        order.append("outer-caught")
    finally:
        order.append("outer-finally")
    return order

print(nested())

def reraise():
    try:
        try:
            raise KeyError("k")
        except KeyError:
            raise
    except KeyError as error:
        return "reraised " + str(error)

print(reraise())

def assertion(value):
    assert value > 0, "must be positive"
    return value

print(assertion(5))

try:
    assertion(-1)
except AssertionError as error:
    print("assert", error)

try:
    raise TypeError
except TypeError as error:
    print("class raise", repr(error))

try:
    raise 42
except TypeError as error:
    print("bad raise", error)

def caught_by_base():
    try:
        raise ZeroDivisionError("x")
    except ArithmeticError as error:
        return ("arithmetic", type(error))

print(caught_by_base())

try:
    try:
        raise ValueError("propagate")
    except KeyError:
        print("wrong handler")
except ValueError as error:
    print("propagated", error)

try:
    raise KeyError("any")
except:
    print("bare except")

def override():
    try:
        return "try"
    finally:
        return "finally"

print(override())

def handled_then_clean():
    steps = []
    try:
        steps.append("body")
    except ValueError:
        steps.append("handler")
    else:
        steps.append("else")
    finally:
        steps.append("finally")
    return steps

print(handled_then_clean())
