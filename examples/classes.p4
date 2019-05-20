class Point:
    x: int
    y: int

    def sum(self: Point) -> int:
        return self.x + self.y

class Metrics:
    label: str
    ok: bool
    sample: (int,float)

    def sample_sum(self: Metrics) -> float:
        return self.sample[0] + self.sample[1]

def main() -> None:
    p: Point = Point(3, 4)
    m: Metrics = Metrics("load", True, (2, 1.5))
    p.x = 10
    m.ok = False
    print(p.x)
    print(p.sum())
    print(m.ok)
    print(m.sample_sum())
