class Point:
    x: int
    y: int

    def __init__(self: Point, x: int, y: int) -> None:
        self.x = x
        self.y = y

points: list[Point] = []

def main() -> None:
    points.append(Point(1, 2))
    points.append(Point(3, 4))
    print(len(points))
    print(points[0])
    print(points[1].x)
    for p in points:
        print(p.y)
    print(points)
