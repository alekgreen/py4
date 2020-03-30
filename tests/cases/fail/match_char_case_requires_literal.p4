def main() -> None:
    needle = 'x'
    value = 'x'

    match value:
        case needle:
            print("hit")
        case _:
            print("miss")
