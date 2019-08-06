_seed: int = 5

def _twice(x: int) -> int:
    return x * 2

class _Point:
    x: int
    y: int

def reveal_total() -> int:
    p: _Point = _Point(2, _seed)
    return _twice(p.x + p.y)
