def choose(x: float) -> float:
    return x

def choose(x: int | float) -> int | float:
    return x

def main() -> None:
    picked = choose(1)
    print(picked)
