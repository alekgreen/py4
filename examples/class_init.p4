class Counter:
    value: int
    label: str

    def __init__(self: Counter, start: int, name: str) -> None:
        self.value = start
        self.label = name

    def bump(self: Counter, delta: int) -> int:
        return self.value + delta

def main() -> None:
    c: Counter = Counter(4, "hits")
    print(c.value)
    print(c.label)
    print(c.bump(3))
    print(c)
