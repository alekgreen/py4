# List-heavy benchmark using append, len, and indexed mutation.

def wrap_97(value: int) -> int:
    while value >= 97:
        value = value - 97
    return value

def wrap_5(value: int) -> int:
    while value >= 5:
        value = value - 5
    return value

def work(size: int, rounds: int) -> int:
    xs: list[int] = []
    i: int = 0
    total: int = 0

    while i < size:
        xs.append(wrap_97(i))
        i = i + 1

    r: int = 0
    while r < rounds:
        j: int = 0
        while j < len(xs):
            xs[j] = xs[j] + wrap_5(j + r)
            total = total + xs[j]
            while total > 1000000:
                total = total - 1000000
            j = j + 1
        r = r + 1

    return total

def main() -> None:
    print(work(2000, 1000))
