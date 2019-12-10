import json


def main() -> None:
    value = json.from_string[int]("1")
    print(value)
