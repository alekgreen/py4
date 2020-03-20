class Point:
    x: int
    y: int

def build_point() -> Point:
    point = Point(2, 5)
    return point

def main() -> None:
    count = 7
    ratio = 2.5
    ok = True
    letter = 'z'
    label = "ready"
    nums = [1, 2, 3]
    pair = (count, label)
    point = build_point()

    print(count)
    print(ratio)
    print(ok)
    print(letter)
    print(label)
    print(nums)
    print(pair)
    print(point)
