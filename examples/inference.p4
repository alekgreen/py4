base = 4
step = 3

class Point:
    x: int
    y: int

def build() -> Point:
    point = Point(base, step)
    return point

def main() -> None:
    total = base + step
    ratio = 2.5
    ok = True
    label = "ready"
    nums = [1, 2, 3]
    point = build()

    print(total)
    print(ratio)
    print(ok)
    print(label)
    print(nums)
    print(point)
