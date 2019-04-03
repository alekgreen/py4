# Arithmetic-heavy loop benchmark kept inside 32-bit-ish bounds.

def work(limit: int) -> int:
    i: int = 0
    a: int = 1
    b: int = 2
    total: int = 0

    while i < limit:
        a = a + b
        while a > 1000000:
            a = a - 1000000

        b = b + total + 3
        while b > 1000000:
            b = b - 1000000

        total = total + a * 3 + b * 2 + i
        while total > 1000000:
            total = total - 1000000

        i = i + 1

    return total

def main() -> None:
    print(work(5000000))
