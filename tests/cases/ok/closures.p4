def main() -> None:
    suffix: str = "!"
    base: int = 7

    def add(x: int) -> int:
        return x + base

    def decorate(value: str) -> str:
        return value + suffix

    print(add(5))
    print(decorate("hi"))
