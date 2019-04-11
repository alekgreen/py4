def sum_list(xs: list[int]) -> int:
    total: int = 0
    for x in xs:
        total = total + x
    return total

def main() -> None:
    xs: list[int] = [1, 2, 3]
    total: int = 0
    for x in xs:
        total = total + x
    print(total)
    print(sum_list([4, 5, 6]))
    for y in [7, 8]:
        print(y)
