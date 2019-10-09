import json


def main() -> None:
    arr = json.parse("[1, 2]")
    print(json.stringify(json.get_index(arr, 5)))
