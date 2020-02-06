def choose(flag: bool) -> int | float:
    if flag:
        return 1
    return 2.5

def main() -> None:
    value: int | float = choose(False)
    print(value)
