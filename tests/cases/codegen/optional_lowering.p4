class Point:
    x: int
    y: int

def maybe(flag: bool) -> Point | None:
    if flag:
        return Point(1, 2)
    return None

def main() -> None:
    point: Point | None = maybe(False)
    print("done")
