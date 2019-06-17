def join_count(xs: list[str]) -> int:
    total: int = 0
    for _ in range(len(xs)):
        total = total + 1
    return total

def main() -> None:
    xs: list[str] = []
    xs.append("red")
    xs.append("blue")
    ys: list[str] = xs.copy()
    ys[1] = "green"
    print(xs)
    print(ys)
    print(ys.pop())
    print(ys)
    print(join_count(xs))
