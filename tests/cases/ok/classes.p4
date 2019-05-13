class Point:
    x: int
    y: int

class Metrics:
    label: str
    ok: bool
    sample: (int,float)

def describe(p: Point, m: Metrics) -> None:
    print("class metadata ready")

def main() -> None:
    print("classes")
