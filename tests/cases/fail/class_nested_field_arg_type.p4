class Point:
    x: int
    y: int

class Segment:
    start: Point
    end: Point

def main() -> None:
    s: Segment = Segment(1, Point(2, 3))
    print(s)
