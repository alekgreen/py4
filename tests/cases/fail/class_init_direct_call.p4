class Counter:
    value: int

    def __init__(self: Counter, start: int) -> None:
        self.value = start

def main() -> None:
    c: Counter = Counter(1)
    c.__init__(2)
