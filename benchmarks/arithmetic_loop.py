ITERATIONS = 5000000


def work(limit: int) -> int:
    i = 0
    a = 1
    b = 2
    total = 0

    while i < limit:
        a = a + b
        while a > 1000000:
            a = a - 1000000

        b = b + total + 3
        while b > 1000000:
            b = b - 1000000

        total = total + (a * 3) + (b * 2) + i
        while total > 1000000:
            total = total - 1000000

        i = i + 1

    return total


if __name__ == "__main__":
    print(work(ITERATIONS))
