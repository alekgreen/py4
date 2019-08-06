_seed: int = 3

def _twice(x: int) -> int:
    return x * 2

class _Point:
    x: int
    y: int

def main() -> None:
    p: _Point = _Point(1, 2)
    print(_twice(_seed + p.x))
    print(p.y)
