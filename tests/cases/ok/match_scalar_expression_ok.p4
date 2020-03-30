def main() -> None:
    n = 3
    ch = 'y'
    flag = False

    n_label = match n:
        case 1: "one"
        case 2: "two"
        case _: "other"

    ch_label = match ch:
        case 'x': "ex"
        case _: "other"

    flag_label = match flag:
        case True: "yes"
        case False: "no"

    print(n_label)
    print(ch_label)
    print(flag_label)
