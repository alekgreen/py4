def pair() -> (int,float):
    return (1, 2.5)

def nested() -> ((int,float),int):
    return ((4, 5.5), 6)

def main() -> None:
    left, right = pair()
    ((inner_left, inner_right), count) = nested()
    print(left)
    print(right)
    print(inner_left)
    print(inner_right)
    print(count)
