def main() -> None:
    for i in range(5):
        xs: list[int] = [i]
        if i == 1:
            continue
        if i == 3:
            break
        print(xs[0])
