def main() -> None:
    count = 3
    marker = 'y'
    enabled = True

    match count:
        case 1:
            print("one")
        case 2:
            print("two")
        case _:
            print("many")

    label = match marker:
        case 'x': "ex"
        case 'y': "why"
        case _: "other"

    verdict = match enabled:
        case True: "enabled"
        case False: "disabled"

    print(label)
    print(verdict)
