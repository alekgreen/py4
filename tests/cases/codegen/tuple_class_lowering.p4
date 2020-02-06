class Point:
    x: int
    y: int

def main() -> None:
    point: Point = Point(1, 2)
    pair: (Point, str) = (point, "ok")
    print(pair)
