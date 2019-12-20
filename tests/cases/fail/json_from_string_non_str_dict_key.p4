import json


def main() -> None:
    counts = json.from_string[dict[int, int]]("{\"1\":1}")
    print(counts)
