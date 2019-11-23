import io


class Point:
    x: int
    y: int


class Holder:
    point: Point | None
    file: io.File | None


def main() -> None:
    path = "/tmp/py4_optional_containers.txt"
    file = io.open(path, "w")
    points: list[Point | None] = [Point(1, 2), None]
    files: dict[str, io.File | None] = {"main": file, "backup": None}
    pair: (str, Point | None) = ("current", None)
    holder = Holder(None, file)

    print(points)
    print(files)
    print(pair)
    print(holder)

    io.close(file)
