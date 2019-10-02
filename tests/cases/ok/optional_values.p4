import io


class Point:
    x: int
    y: int


def maybe_point(flag: bool) -> Point | None:
    if flag:
        return Point(1, 2)
    return None


def maybe_file(path: str, flag: bool) -> io.File | None:
    if flag:
        return io.open(path, "w")
    return None


def use_point(point: Point | None) -> None:
    assert True


def use_file(file: io.File | None) -> None:
    assert True


def main() -> None:
    point = maybe_point(True)
    missing: Point | None = None
    file = maybe_file("/tmp/py4_optional_test.txt", True)
    closed: io.File | None = None
    use_point(point)
    use_point(missing)
    use_file(file)
    use_file(closed)
    print("ok")
