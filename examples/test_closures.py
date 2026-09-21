def counter(start):
    total = start

    def step(amount=1):
        nonlocal total
        total = total + amount
        return total

    def current():
        return total

    return step, current

step, current = counter(10)
print(current(), step(), step(5), current())

def make_account(balance):
    history = []

    def deposit(amount):
        nonlocal balance
        balance = balance + amount
        history.append(("deposit", amount))
        return balance

    def withdraw(amount):
        nonlocal balance
        if amount > balance:
            return "insufficient"
        balance = balance - amount
        history.append(("withdraw", amount))
        return balance

    def report():
        return (balance, len(history))

    return deposit, withdraw, report

deposit, withdraw, report = make_account(100)
print(deposit(50), withdraw(30), report())
print(withdraw(1000), report())

def outer_value(value):
    def middle():
        def inner():
            return value * 2
        return inner
    return middle

print(outer_value(21)()())

def shared_cell():
    value = 1

    def bump():
        nonlocal value
        value = value + 1

    def read():
        return value

    bump()
    bump()
    return read()

print(shared_cell())

def loop_capture():
    fns = []
    for index in range(3):
        fns.append(lambda: index)
    return [fn() for fn in fns]

print(loop_capture())

def comp_capture():
    fns = [lambda: item for item in range(3)]
    return [fn() for fn in fns]

print(comp_capture())
