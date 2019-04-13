def take(xs: list[float]) -> int:
    return len(xs)

def build() -> list[float]:
    return []

def main() -> None:
    xs: list[float] = []
    print(len(xs))
    print(take([]))
    print(len(build()))
