def main() -> None:
    i: int = 0
    while i < 6:
        i = i + 1
        if i == 2:
            continue
        if i == 5:
            break
        print(i)

    total: int = 0
    for x in range(6):
        if x == 1:
            continue
        if x == 4:
            break
        total = total + x

    print(total)
