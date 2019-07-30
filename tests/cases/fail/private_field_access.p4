class Counter:
    __value: int

def main() -> None:
    c: Counter = Counter(4)
    print(c.__value)
