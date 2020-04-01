def main() -> None:
    count: int = 0

    def bump() -> int:
        count = count + 1
        return count

    print(bump())
