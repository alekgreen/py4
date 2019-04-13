def sum_list(xs: list[float]) -> float:
    total: float = 0.0
    for x in xs:
        total = total + x
    return total

def main() -> None:
    xs: list[float] = []
    xs.append(1.5)
    xs.append(2.0)
    xs.append(3)
    print(len(xs))
    print(xs[0])
    xs[1] = 4
    print(xs[1])
    ys: list[float] = xs.copy()
    print(ys.pop())
    ys.clear()
    print(len(ys))
    print(sum_list(xs))
    zs: list[float] = []
    print(len(zs))
