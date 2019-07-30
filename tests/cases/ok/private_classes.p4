class Counter:
    __value: int
    label: str

    def __bump(self: Counter) -> int:
        self.__value = self.__value + 1
        return self.__value

    def reveal(self: Counter) -> int:
        return self.__bump()

def main() -> None:
    c: Counter = Counter(4, "hits")
    print(c.reveal())
    print(c.label)
