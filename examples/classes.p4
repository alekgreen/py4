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

class Segment:
    start: Point
    end: Point

    def span(self: Segment) -> int:
        return (self.end.x - self.start.x) + (self.end.y - self.start.y)

def main() -> None:
    p: Point = Point(3, 4)
    m: Metrics = Metrics("load", True, (2, 1.5))
    s: Segment = Segment(Point(1, 2), Point(5, 9))
    p.x = 10
    m.ok = False
    s.start.x = 2
    print(p.x)
    print(p.sum())
    print(m.ok)
    print(m.sample_sum())
    print(s.span())
    print(s.start.x)
    print(p)
    print(m)
    print(s)
