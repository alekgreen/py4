def main() -> None:
    suffix = "!"
    base = 7

    def add(x: int) -> int:
        return x + base

    def decorate(value: str) -> str:
        return value + suffix

    print(add(5))
    print(decorate("hi"))
