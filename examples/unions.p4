def choose(flag: bool, left: int, right: float) -> int | float:
    if flag:
        return left
    return right

def maybe_word(flag: bool) -> str | None:
    if flag:
        return "yes"
    return

def echo(value: int | float) -> int | float:
    return value

def main() -> None:
    numeric: int | float = choose(True, 7, 2.5)
    print(numeric)
    numeric = echo(2.5)
    print(numeric)

    maybe: str | None = maybe_word(True)
    print(maybe)
    maybe = maybe_word(False)
    print(maybe)
