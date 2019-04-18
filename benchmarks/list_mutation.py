LIST_SIZE = 2000
ROUNDS = 1000


def wrap_97(value: int) -> int:
    while value >= 97:
        value -= 97
    return value


def wrap_5(value: int) -> int:
    while value >= 5:
        value -= 5
    return value


def work(size: int, rounds: int) -> int:
    xs = []
    i = 0
    total = 0

    while i < size:
        xs.append(wrap_97(i))
        i += 1

    r = 0
    while r < rounds:
        j = 0
        while j < len(xs):
            xs[j] = xs[j] + wrap_5(j + r)
            total += xs[j]
            while total > 1000000:
                total -= 1000000
            j += 1
        r += 1

    return total


if __name__ == "__main__":
    print(work(LIST_SIZE, ROUNDS))
