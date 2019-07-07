import math

def twice(x: int) -> int:
    return x + x

def twice(x: float) -> float:
    return x + x

def widen(x: float) -> float:
    return x + 0.5

def main() -> None:
    print(math.abs(-5))
    print(math.abs(-3.5))
    print(twice(4))
    print(twice(1.25))
    print(widen(2))
