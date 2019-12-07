import json


def main() -> None:
    value = json.parse("[1, 2]")
    print(json.values(value))
