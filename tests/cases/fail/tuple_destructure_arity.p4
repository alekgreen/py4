def pair() -> (int,float):
    return (1, 2.5)

def main() -> None:
    (a, b, c): (int,float,float) = (1, 2.5, 3.0)
    (a, b) = pair()
    (a, b, c) = pair()
