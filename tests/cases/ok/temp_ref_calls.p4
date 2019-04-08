def first_plus_len(xs: list[int]) -> int:
    return xs[0] + len(xs)

def make_numbers() -> list[int]:
    return [7, 8, 9]

def main() -> None:
    print(first_plus_len([7, 8, 9]))
    print(len(make_numbers()))
    print([4, 5][0])
