def seed(xs: list[int], base: int) -> None:
    list_append(xs, base)
    list_append(xs, base + 1)

def make_numbers() -> list[int]:
    xs: list[int] = list_int()
    seed(xs, 10)
    list_append(xs, 20)
    return xs

def main() -> None:
    xs: list[int] = make_numbers()
    print(list_len(xs))
    print(list_get(xs, 0))
    print(list_get(xs, 2))
    list_set(xs, 1, 99)
    print(list_get(xs, 1))
