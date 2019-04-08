def seed(xs: list[int], base: int) -> None:
    list_append(xs, base)
    list_append(xs, base + 1)

def make_numbers() -> list[int]:
    xs: list[int] = [10]
    seed(xs, 10)
    list_append(xs, 20)
    return xs

def main() -> None:
    empty: list[int] = []
    print(len(empty))
    xs: list[int] = make_numbers()
    print(len(xs))
    print(xs[0])
    print(xs[2])
    xs[1] = 99
    print(xs[1])
    print(len([7, 8, 9]))
    print(make_numbers()[0])
