class Point:
    x: int
    y: int

class Metrics:
    label: str
    ok: bool
    sample: (int,float)

def point_sum(p: Point) -> int:
    return p.x + p.y

def metric_sample_sum(m: Metrics) -> float:
    return m.sample[0] + m.sample[1]

def main() -> None:
    p: Point = Point(3, 4)
    m: Metrics = Metrics("load", True, (2, 1.5))
    p.x = 10
    m.ok = False
    print(p.x)
    print(point_sum(p))
    print(m.ok)
    print(metric_sample_sum(m))
