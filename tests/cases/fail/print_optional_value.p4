class Point:
    x: int
    y: int


def maybe_point(flag: bool) -> Point | None:
    if flag:
        return Point(1, 2)
    return None


def main() -> None:
    point = maybe_point(True)
    print(point)
