class Point:
    x: int
    y: int

def main() -> None:
    points: dict[str, Point] = {"a": Point(1, 2), "b": Point(3, 4)}

    print(points)
    print(points["a"])
    print(points.get_or("z", Point(9, 9)))
    print(points.items())
