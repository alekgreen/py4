class Point:
    x: int
    y: int

    def sum(self: Point) -> int:
        return self.x + self.y

def make_entry(p: Point, label: str) -> (Point,str):
    return (p, label)

def main() -> None:
    entry: (Point,str) = make_entry(Point(2, 5), "origin-ish")
    (pt, name): (Point,str) = entry
    print(pt.x)
    print(pt.sum())
    print(name)
    print(entry)
