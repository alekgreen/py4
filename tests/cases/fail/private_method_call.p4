class Counter:
    __value: int

    def __bump(self: Counter) -> int:
        return self.__value + 1

def main() -> None:
    c: Counter = Counter(4)
    print(c.__bump())
