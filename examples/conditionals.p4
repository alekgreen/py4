def classify(x: int) -> str:
    if x > 5:
        return "big"
    elif x == 5:
        return "mid"
    else:
        return "small"

def main() -> None:
    print(classify(3))
    print(classify(5))
    print(classify(8))
