counter: int = 0

def bump() -> int:
    counter = counter + 1
    return counter

def main() -> None:
    print(0 < bump() < 3)
    print(counter)
