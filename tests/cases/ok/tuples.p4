def make_pair(a: int, b: float) -> (int,float):
    return (a, b)

def first(pair: (int,float)) -> int:
    return pair[0]

def second(pair: (int,float)) -> float:
    return pair[1]

def main() -> None:
    pair: (int,float) = make_pair(3, 2.5)
    (left, right): (int,float) = make_pair(7, 1.25)
    a: int = 0
    b: float = 0.0
    (a, b) = pair
    nested: ((int,float),int) = ((4, 5.5), 6)
    ((inner_a, inner_b), inner_c): ((int,float),int) = nested
    widened: (float,float) = (1, 2)
    print(first(pair))
    print(second(pair))
    print(left)
    print(right)
    print(a)
    print(b)
    print(inner_a)
    print(inner_b)
    print(inner_c)
    print(pair)
    print(nested)
    print(widened[0])
    print(widened[1])
