def main() -> None:
    n = 2
    ch = 'x'
    flag = True

    match n:
        case 1:
            print("one")
        case 2:
            print("two")
        case _:
            print("other")

    match ch:
        case 'x':
            print("ex")
        case _:
            print("other")

    match flag:
        case True:
            print("yes")
        case False:
            print("no")
