def make_pair(a: int, b: float) -> (int,float):
    return (a, b)

def first(pair: (int,float)) -> int:
    return pair[0]

def second(pair: (int,float)) -> float:
    return pair[1]

def main() -> None:
    pair: (int,float) = make_pair(3, 2.5)
    widened: (float,float) = (1, 2)
    print(first(pair))
    print(second(pair))
    print(widened[0])
    print(widened[1])
