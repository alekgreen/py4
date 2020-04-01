def main() -> None:
    base: int = 7

    def add(x: int) -> int:
        return x + base

    print(add(5))
