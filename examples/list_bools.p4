def count_true(xs: list[bool]) -> int:
    total: int = 0
    for flag in xs:
        if flag:
            total = total + 1
    return total

def main() -> None:
    xs: list[bool] = []
    xs.append(True)
    xs.append(False)
    xs.append(True)
    print(len(xs))
    print(xs[0])
    xs[1] = True
    print(xs[1])
    ys: list[bool] = xs.copy()
    print(ys.pop())
    ys.clear()
    print(len(ys))
    print(count_true(xs))
    zs: list[bool] = [False, True]
    print(zs[0])
